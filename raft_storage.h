#ifndef raft_storage_h
#define raft_storage_h

#include "raft_protocol.h"
#include <fcntl.h>
#include <mutex>
#include <iostream>
#include <fstream>

template<typename command>
class raft_storage {
public:
    raft_storage(const std::string& file_dir);
    ~raft_storage();
    // Your code here
    void Append(command cmd) {

        mtx.lock();

        std::ofstream outfile(file_path, std::ios::out | std::ios::app);
        outfile << cmd << std::endl;

        mtx.unlock();

        return;
    };
    // std::string file_path;
private:
    std::mutex mtx;
    std::string file_path;
};

template<typename command>
raft_storage<command>::raft_storage(const std::string& dir){
    // Your code here
    file_path = dir;
}

template<typename command>
raft_storage<command>::~raft_storage() {
   // Your code here
}

#endif // raft_storage_h