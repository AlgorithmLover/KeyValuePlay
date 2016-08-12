//
// Created by cheyulin on 8/10/16.
//

#ifndef KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H
#define KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

using namespace std;

#define FILE_NAME "tuple_transaction.db"
#define SEPERATOR ","
#define SEPERATOR_END_CHAR ';'
#define SEPERATOR_END_STRING ";"
#define SEPERATOR_CHAR ','

class Answer {
private:
    unordered_map<string, string> yche_map_;
    fstream output_file_stream_;
    size_t count{0};
    bool first_time{true};

    inline pair<string, string> split(const string &str) {
        auto iter_begin = str.begin();
        auto iter_end = str.end();
        auto iter_middle = find(iter_begin, iter_end, ',');
        return std::move(make_pair(std::move(string(iter_begin, iter_middle)),
                                   std::move(string(iter_middle + 1, iter_end - 1))));
    }

public: //put和get方法要求public
    Answer() {
        yche_map_.reserve(5000000);
        fstream input_file_stream{FILE_NAME, ifstream::in};

        if (input_file_stream.is_open()) {
            input_file_stream.seekg(0, ios::end);
            size_t buffer_size = input_file_stream.tellg();
            input_file_stream.seekg(0, std::ios::beg);
            char *file_content = new char[buffer_size];
            input_file_stream.read(file_content, buffer_size);
            input_file_stream.close();

            stringstream str_stream(file_content);
            string tmp_string;
            for (; str_stream.good();) {
                getline(input_file_stream, tmp_string);
                if (tmp_string.size() > 0 && tmp_string.substr(tmp_string.size() - 1) == SEPERATOR_END_STRING) {
                    auto my_pair = std::move(split(tmp_string));
                    yche_map_[std::move(my_pair.first)] = std::move(my_pair.second);
                }
            }
            delete[](file_content);
        } else {
            input_file_stream.close();
        }

        if (yche_map_.size() >= 100000) {
            first_time = false;
        }
        output_file_stream_.open(FILE_NAME, std::ofstream::out | std::ofstream::app);
    }

    inline string get(string key) { //读取KV
        auto iter = yche_map_.find(key);
        if (iter != yche_map_.end()) {
            return iter->second;
        }
        else {
            return "NULL"; //文件不存在，说明该Key不存在，返回NULL
        }
    }

    inline void put(string key, string value) { //存储KV
        yche_map_[key] = value;
        ++count;
        output_file_stream_ << key << SEPERATOR << value << SEPERATOR_END_CHAR << '\n';
        if (first_time) {
            if (yche_map_.size() < 100000) {
                output_file_stream_ << flush;
            }
            else {
                first_time = false;
                count = 0;
            }
        }
        else if (count > 5000) {
            output_file_stream_ << flush;
            count = 0;
        }
    }
};

#endif //KEYVALUESTORE_PURE_MEMORY_KEY_VALUE_STORE_H
