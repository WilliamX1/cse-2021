#ifndef raft_storage_h
#define raft_storage_h

#include "raft_protocol.h"
#include <fcntl.h>
#include <mutex>
#include <fstream>

template<typename command>
class raft_storage {
public:
    raft_storage(const std::string& file_dir);
    ~raft_storage();
    // Your code here
    int current_term;
    int vote_for;
    std::vector<log_entry<command>> log;

    void flush();

private:
    std::mutex mtx;
    std::string file_path;
};

template<typename command>
raft_storage<command>::raft_storage(const std::string& dir){
    // Your code here
    file_path = dir + "/data";
    std::ifstream in(file_path, std::ios::in | std::ios::binary);
    if (in.is_open()) {
        in.read((char *) &current_term, sizeof(int));
        in.read((char *) &vote_for, sizeof(int));

        int log_size;
        in.read((char *) &log_size, sizeof(int));

        log.clear();
        for (int i = 0; i < log_size; ++i) {
            log_entry<command> new_entry;
            int size;
            char *buf;

            in.read((char *) &new_entry.term, sizeof(int));
            in.read((char *) &size, sizeof(int));

            buf = new char [size];
            in.read(buf, size);

            new_entry.cmd.deserialize(buf, size);
            log.push_back(new_entry);
            delete [] buf;
        }
    }
    else {
        current_term = 0;
        vote_for = -1;
        log.clear();
    }
    in.close();
}

template<typename command>
raft_storage<command>::~raft_storage() {
    // Your code here
    flush();
}

template<typename command>
void raft_storage<command>::flush() {
    mtx.lock();
    std::ofstream out(file_path, std::ios::trunc | std::ios::out | std::ios::binary);
    if (out.is_open()) {
        out.write((char *) &current_term, sizeof(int));
        out.write((char *) &vote_for, sizeof(int));
        int log_size = log.size();
        out.write((char *) &log_size, sizeof(int));

        for (auto &it : log) {
            int size = it.cmd.size();
            char *buf = new char [size];

            it.cmd.serialize(buf, size);

            out.write((char *) &it.term, sizeof(int));
            out.write((char *) &size, sizeof(int));
            out.write(buf, size);
            delete [] buf;
        }
    }
    out.close();
    mtx.unlock();
}

#endif // raft_storage_h