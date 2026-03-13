#ifndef STORAGE_HPP
#define STORAGE_HPP

#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int CACHE_SIZE = 10000;

struct Record {
    char key[65];
    int value;

    Record() { memset(key, 0, sizeof(key)); value = 0; }
    Record(const string& k, int v) : value(v) {
        strncpy(key, k.c_str(), 64);
        key[64] = '\0';
    }

    bool operator<(const Record& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
};

class Storage {
private:
    multimap<string, int> mem_data;
    string filename;
    bool dirty;
    int record_count;

    void load_from_file() {
        ifstream fin(filename, ios::binary);
        if (!fin.is_open()) return;

        fin.read(reinterpret_cast<char*>(&record_count), sizeof(int));

        for (int i = 0; i < record_count; i++) {
            Record rec;
            fin.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            mem_data.insert({string(rec.key), rec.value});
        }

        fin.close();
    }

    void save_to_file() {
        if (!dirty) return;

        ofstream fout(filename, ios::binary | ios::trunc);

        record_count = mem_data.size();
        fout.write(reinterpret_cast<const char*>(&record_count), sizeof(int));

        for (auto& p : mem_data) {
            Record rec(p.first, p.second);
            fout.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
        }

        fout.close();
        dirty = false;
    }

public:
    Storage(const string& fname) : filename(fname), dirty(false), record_count(0) {
        load_from_file();
    }

    ~Storage() {
        save_to_file();
    }

    void insert(const string& key, int value) {
        mem_data.insert({key, value});
        dirty = true;

        if (mem_data.size() % CACHE_SIZE == 0) {
            save_to_file();
        }
    }

    void remove(const string& key, int value) {
        auto range = mem_data.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == value) {
                mem_data.erase(it);
                dirty = true;
                break;
            }
        }
    }

    vector<int> find(const string& key) {
        vector<int> result;
        auto range = mem_data.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        sort(result.begin(), result.end());
        return result;
    }
};

#endif
