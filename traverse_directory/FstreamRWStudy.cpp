//
// Created by cheyulin on 8/14/16.
//

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

#define FLAG_ALIGNMENT 1
#define KEY_ALIGNMENT 10
#define VALUE_ALIGNMENT 20
#define YCHE_FILE "hello_fstream.txt"

using namespace std;

int main() {
    string s1 = "123";
    fstream fstream1(YCHE_FILE, ios::in | ios::out | ios::app);
    fstream1 << "1:" << left << setw(KEY_ALIGNMENT) << s1 << left << setw(KEY_ALIGNMENT) << "34234" << ";\n";
    fstream1 << "2:" << left << setw(KEY_ALIGNMENT) << s1 << left << setw(KEY_ALIGNMENT) << "321321" << ";\n";

    fstream1.seekg(0, ios::beg);
    string tmp_string;
    for (; !fstream1.eof();) {
        getline(fstream1, tmp_string);
        cout << tmp_string << endl;
    }
}

