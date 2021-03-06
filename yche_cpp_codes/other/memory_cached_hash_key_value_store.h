//
// Created by cheyulin on 8/10/16.
//

#ifndef KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H
#define KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace std;

#define HASH_FUNC(x) hash_func(x)

constexpr unsigned int DEFAULT_HASH_TABLE_SLOT_SIZE = 80000;
constexpr double LOAD_FACTOR_THRESHOLD = 0.8;
constexpr unsigned int EXTRA_ALIGNMENT_SIZE = 1;

#define EXTRA_SPLIT_STRING  "\n"
#define SMALL_FILE_NAME  "small.db"
#define MEDIUM_FILE_NAME  "medium.db"
#define LARGE_FILE_NAME "large.db"

#define SMALL_INDEX_FILE_NAME "index_small.txt"
#define MEDIUM_INDEX_FILE_NAME "index_medium.txt"
#define LARGE_INDEX_FILE_NAME "index_large.txt"

enum class KeyAlignment {
    small = 70,
    medium = 300,
    large = 3000
};

enum class ValueAlignment {
    small = 160,
    medium = 3000,
    large = 30000
};

enum class HashFileSlotSize {
    small = 100000,
    medium = 1000000,
    large = 100000

};

enum class HashInMemoryMaxSize {
    small = 100000,
    medium = 90000,
    large = 4000
};

enum class DataSetType {
    small,
    medium,
    large
};

struct DataSetAlignmentInfo {
    size_t key_alignment_size_;
    size_t value_alignment_size_;
    size_t whole_alignment_size_;

    size_t hash_in_memory_tuple_size_;
    size_t hash_file_slot_size_;

    DataSetAlignmentInfo(DataSetType dataset_type) {
        if (dataset_type == DataSetType::small) {
            key_alignment_size_ = static_cast<size_t>(KeyAlignment::small);
            value_alignment_size_ = static_cast<size_t>(ValueAlignment::small);
            hash_file_slot_size_ = static_cast<size_t>(HashFileSlotSize::small);
            hash_in_memory_tuple_size_ = static_cast<size_t>(HashInMemoryMaxSize::small);
        } else if (dataset_type == DataSetType::medium) {
            key_alignment_size_ = static_cast<size_t>(KeyAlignment::medium);
            value_alignment_size_ = static_cast<size_t>(ValueAlignment::medium);
            hash_file_slot_size_ = static_cast<size_t>(HashFileSlotSize::medium);
            hash_in_memory_tuple_size_ = static_cast<size_t>(HashInMemoryMaxSize::medium);
        } else {
            key_alignment_size_ = static_cast<size_t>(KeyAlignment::large);
            value_alignment_size_ = static_cast<size_t>(ValueAlignment::large);
            hash_file_slot_size_ = static_cast<size_t>(HashFileSlotSize::large);
            hash_in_memory_tuple_size_ = static_cast<size_t>(HashInMemoryMaxSize::large);
        }
        whole_alignment_size_ = key_alignment_size_ + value_alignment_size_ + EXTRA_ALIGNMENT_SIZE;
    }
};

inline void trim_right_blank(string &to_trim_string) {
    auto iter_back = std::find_if_not(to_trim_string.rbegin(), to_trim_string.rend(),
                                      [](int c) { return std::isspace(c); }).base();
    to_trim_string = std::string(to_trim_string.begin(), iter_back);
}

void SerializeInt32(char (&buf)[4], int32_t val) {
    std::memcpy(buf, &val, 4);
}

int32_t ParseInt32(const char (&buf)[4]) {
    int32_t val;
    std::memcpy(&val, buf, 4);
    return val;
}

std::hash<string> hash_func;

template<size_t slot_num = DEFAULT_HASH_TABLE_SLOT_SIZE>
class yche_map {
private:
    vector<pair<string, string>> hash_table_;
    string value_result_string_;
    size_t current_size_{0};
    size_t max_slot_size_{slot_num};

    //structure for persistence
    bool is_current_in_memory_table_full_{false};

    inline void rebuild() {
        vector<pair<string, string>> my_hash_table_building;
        max_slot_size_ *= 2;
        my_hash_table_building.resize(max_slot_size_);
        for (size_t previous_index = 0; previous_index < max_slot_size_ / 2; ++previous_index) {
            if (hash_table_[previous_index].first.size() > 0) {
                auto new_index = HASH_FUNC(hash_table_[previous_index].first) % max_slot_size_;
                for (; my_hash_table_building[new_index].first.size() != 0;
                       new_index = (++new_index) % max_slot_size_) {
                }
                my_hash_table_building[new_index] = std::move(hash_table_[previous_index]);
            }

        }
        hash_table_ = std::move(my_hash_table_building);
    }

    inline void write_key_value_to_file(const string &key, const string &value, const size_t &hash_result) {
        auto &hash_file_slot_size = data_set_alignment_info_ptr_->hash_file_slot_size_;
        auto &key_alignment_size = data_set_alignment_info_ptr_->key_alignment_size_;
        auto &value_alignment_size = data_set_alignment_info_ptr_->value_alignment_size_;
        auto &whole_alignment_size = data_set_alignment_info_ptr_->whole_alignment_size_;
        //linear probing
        string key_string;

        for (auto index = hash_result % hash_file_slot_size;; index = (++index) % hash_file_slot_size) {
            db_file_stream_ptr_->seekg(index * whole_alignment_size, ios::beg);
            db_file_stream_ptr_->read(read_buffer_, key_alignment_size);
            key_string = std::move(string(read_buffer_, 0, key_alignment_size));
            trim_right_blank(key_string);
            if (key_string.size() == 0) {
                SerializeInt32(index_chars_array_, index);
                index_file_stream_ptr->write(index_chars_array_, 4);
            }
            if (key_string.size() == 0 || key_string == key) {
                db_file_stream_ptr_->seekp(index * whole_alignment_size, ios::beg);
                (*db_file_stream_ptr_) << left << setw(key_alignment_size) << key << left << setw(value_alignment_size)
                                       << value << EXTRA_SPLIT_STRING;
                break;
            }
        }
    }

    inline string *read_file_search_key(const string &key, const size_t &hash_result) {
        auto &hash_file_slot_size = data_set_alignment_info_ptr_->hash_file_slot_size_;
        auto &key_alignment_size = data_set_alignment_info_ptr_->key_alignment_size_;
        auto &whole_alignment_size = data_set_alignment_info_ptr_->whole_alignment_size_;
        //linear probing
        string key_string;
        for (auto search_index = hash_result % hash_file_slot_size;; search_index =
                                                                             (++search_index) % hash_file_slot_size) {
            db_file_stream_ptr_->seekg(search_index * whole_alignment_size, ios::beg);
            db_file_stream_ptr_->read(read_buffer_, whole_alignment_size);
            key_string = std::move(string(read_buffer_, 0, key_alignment_size));
            trim_right_blank(key_string);
            if (key_string.size() == 0) {
                return nullptr;
            } else if (key_string == key) {
                value_result_string_ = std::move(string(read_buffer_, key_alignment_size, whole_alignment_size));
                trim_right_blank(value_result_string_);
                return &value_result_string_;
            }
        }
    }

public:
    DataSetAlignmentInfo *data_set_alignment_info_ptr_{nullptr};
    fstream *db_file_stream_ptr_{nullptr};
    fstream *index_file_stream_ptr{nullptr};

    char *read_buffer_{nullptr};
    char index_chars_array_[4];
    vector<int32_t> index_array_;

    yche_map() : hash_table_(slot_num) {}

    virtual ~yche_map() {
        delete[]read_buffer_;
    }

    void inline resize(size_t size) {
        hash_table_.resize(size);
    }

    void inline read_indices_into_index_array() {
        index_file_stream_ptr->seekg(0, ios::end);
        auto length = index_file_stream_ptr->tellg();
        index_array_.resize(length / 4);
        for (auto i = 0; i < length / 4; i++) {
            index_file_stream_ptr->seekg(4 * i, ios::beg);
            index_file_stream_ptr->read(index_chars_array_, 4);
            if (index_file_stream_ptr->good()) {
                index_array_[i] = ParseInt32(index_chars_array_);
            }
        }
    }

    void inline read_portion_of_file_to_hash_table() {
        read_indices_into_index_array();
        index_file_stream_ptr->clear();
        auto &whole_alignment_size = data_set_alignment_info_ptr_->whole_alignment_size_;
        auto &key_alignment_size = data_set_alignment_info_ptr_->key_alignment_size_;
        string key_string;
        string value_string;
        for (auto &index:index_array_) {
            db_file_stream_ptr_->seekg(index * whole_alignment_size, ios::beg);
            db_file_stream_ptr_->read(read_buffer_, whole_alignment_size);
            key_string = std::move(string(read_buffer_, 0, key_alignment_size));
            trim_right_blank(key_string);
            value_string = std::move(string(read_buffer_, key_alignment_size, whole_alignment_size));
            trim_right_blank(value_string);
            insert_or_replace(key_string, value_string);
        }
    }

    inline size_t size() {
        return current_size_;
    }

    inline string *get(const string &key) {
        auto hash_result = HASH_FUNC(key);
        //linear probing, judge if there is a existence in memory
        for (auto index = hash_result % max_slot_size_;
             hash_table_[index].first.size() != 0; index = (++index) % max_slot_size_) {
            if (hash_table_[index].first == key) {
                return &hash_table_[index].second;
            }
        }
        //if in-memory hash_table is full search in file, read file
        if (is_current_in_memory_table_full_ == true)
            return read_file_search_key(key, hash_result);
        return nullptr;
    }

    inline void insert_or_replace(const string &key, const string &value) {
        auto hash_result = HASH_FUNC(key);
        //write to the db file for persistence
        write_key_value_to_file(key, value, hash_result);

        //update in-memory hash table
        auto index = hash_result % max_slot_size_;
        for (; hash_table_[index].first.size() != 0; index = (++index) % max_slot_size_) {
            if (hash_table_[index].first == key) {
                hash_table_[index].second = value;
                return;
            }
        }
        //not hit in memory, judge whether memory is full to decide whether continuing insertion
        if (is_current_in_memory_table_full_ == false) {
            hash_table_[index].first = key;
            hash_table_[index].second = value;
            ++current_size_;
            if (current_size_ / max_slot_size_ > LOAD_FACTOR_THRESHOLD) {
                if (max_slot_size_ * 2 < data_set_alignment_info_ptr_->hash_in_memory_tuple_size_)
                    rebuild();
                else
                    is_current_in_memory_table_full_ = true;
            }
        }
    }
};

class Answer {
private:
    yche_map<> map_;
    bool is_file_exists_{false};
    fstream db_file_stream_;
    fstream index_file_stream_;
    DataSetAlignmentInfo *data_set_alignment_info_ptr_{nullptr};

    void inline create_db_file(string filename) {
        db_file_stream_.open(filename, ios::out | ios::trunc);
        db_file_stream_ << left << setw(data_set_alignment_info_ptr_->whole_alignment_size_ *
                                        data_set_alignment_info_ptr_->hash_file_slot_size_) << "";
        db_file_stream_.close();
        db_file_stream_.open(filename, ios::in | ios::out);
    }

    void inline open_index_file(string filename) {
        index_file_stream_.open(filename, ios::in | ios::out | ios::app);
    }

    void inline init_yche_hash_map(DataSetType data_set_type) {
        if (data_set_type == DataSetType::small) {
            open_index_file(SMALL_INDEX_FILE_NAME);
        } else if (data_set_type == DataSetType::medium) {
            open_index_file(MEDIUM_INDEX_FILE_NAME);
        } else {
            open_index_file(LARGE_INDEX_FILE_NAME);
        }
        data_set_alignment_info_ptr_ = new DataSetAlignmentInfo(data_set_type);
        map_.data_set_alignment_info_ptr_ = data_set_alignment_info_ptr_;
        map_.db_file_stream_ptr_ = &db_file_stream_;
        map_.index_file_stream_ptr = &index_file_stream_;
        map_.read_buffer_ = new char[data_set_alignment_info_ptr_->whole_alignment_size_];
        map_.resize(data_set_alignment_info_ptr_->hash_in_memory_tuple_size_);
    }

public:
    Answer() {
        db_file_stream_.open(SMALL_FILE_NAME, ios::in | ios::out);
        is_file_exists_ = true;
        if (db_file_stream_.good()) {
            init_yche_hash_map(DataSetType::small);
            map_.read_portion_of_file_to_hash_table();
        } else {
            db_file_stream_.open(MEDIUM_FILE_NAME, ios::in | ios::out);
            if (db_file_stream_.good()) {
                init_yche_hash_map(DataSetType::medium);
                map_.read_buffer_ = new char[data_set_alignment_info_ptr_->whole_alignment_size_];
                map_.read_portion_of_file_to_hash_table();
            }
            else {
                db_file_stream_.open(LARGE_FILE_NAME, ios::in | ios::out);
                if (db_file_stream_.good()) {
                    init_yche_hash_map(DataSetType::large);
                    map_.read_portion_of_file_to_hash_table();
                }
                else
                    is_file_exists_ = false;
            }
        }
    }

    virtual ~Answer() {
        delete data_set_alignment_info_ptr_;
    }

    inline string get(string &&key) { //读取KV
        auto result = map_.get(key);
        if (result != nullptr) {
            return *result;
        }
        else {
            return "NULL"; //文件不存在，说明该Key不存在，返回NULL
        }
    }

    inline void put(string &&key, string &&value) { //存储KV
        if (is_file_exists_ == false) {
            is_file_exists_ = true;
            auto value_size = value.size();
            if (value_size <= static_cast<size_t>(ValueAlignment::small)) {
                init_yche_hash_map(DataSetType::small);
                create_db_file(SMALL_FILE_NAME);
            } else if (value_size <= static_cast<size_t>(ValueAlignment::medium)) {
                init_yche_hash_map(DataSetType::medium);
                create_db_file(MEDIUM_FILE_NAME);
            }
            else {
                init_yche_hash_map(DataSetType::large);
                create_db_file(LARGE_FILE_NAME);
            }
        }
        map_.insert_or_replace(key, value);
        db_file_stream_ << flush;
        index_file_stream_ << flush;
    }
};

#endif //KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H