#ifndef raft_storage_h
#define raft_storage_h

#include "raft_protocol.h"
#include <fcntl.h>
#include <mutex>

template<typename command>
class raft_storage {
public:
    raft_storage(const std::string& file_dir);
    ~raft_storage();
    // Your code here
private:
    std::mutex mtx;
};

template<typename command>
raft_storage<command>::raft_storage(const std::string& dir){
    // Your code here
}

template<typename command>
raft_storage<command>::~raft_storage() {
   // Your code here
}

#endif // raft_storage_h