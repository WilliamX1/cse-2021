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
    
    /* change/persist log data when:
     * the leader receives a new command or
     * follower receives an append_entries RPC 
     */
    void persist_logdata();

    /* change/persist meta data when
     * the term changes or
     * the node votes for someone
     */

private:
    std::mutex mtx;
    std::string file_dir;
    std::string file_path_metadata;
    std::string file_path_logdata;
};

template<typename command>
raft_storage<command>::raft_storage(const std::string& dir){
    // Your code here
    file_dir = dir;
    file_path_metadata = file_dir + "/metadata.log";
    file_path_logdata = file_dir + "/logdata.log";
}

template<typename command>
raft_storage<command>::~raft_storage() {
   // Your code here

}

template<typename command>
void raft_storage<command>::persist_logdata() {

};

// template<typename command>
// void ra

#endif // raft_storage_h