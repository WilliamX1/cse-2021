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
    void persist_logdata(const log_entry<command> log);

    /* change/persist meta data when
     * the term changes or
     * the node votes for someone
     */
    void persist_metadata(const int currentTerm, const int votedFor);

    /* restore */
    void restore_logdata(std::vector< log_entry<command> >& log);

    void restore_metadata(int& currentTerm, int& votedFor);

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
void raft_storage<command>::persist_logdata(const log_entry<command> log) {
    mtx.lock();
    std::ofstream logdata;
    logdata.open(file_path_logdata, std::ios::out | std::ios::app | std::ios::binary);
    if (logdata.is_open()) {
        logdata.write((char*)& log.term, sizeof(int));
        logdata.write((char*)& log.index, sizeof(int));
        
        int size = log.cmd.size();
        logdata.write((char*)& size, sizeof(int));

        char* buf = new char[size];
        (log.cmd).serialize(buf, size);
        logdata.write(buf, size);
        
        fprintf(stderr, "persist_logdata: log.term: %d, log.index: %d, log.cmd: %d\n", log.term, log.index, log.cmd.value);

        delete [] buf;

        logdata.close();
    };
    mtx.unlock();
    return;
};

template<typename command>
void raft_storage<command>::restore_logdata(std::vector< log_entry<command> >& log) {
    std::ifstream logdata;
    logdata.open(file_path_logdata, std::ios::in | std::ios::binary);
    if (logdata.is_open()) {
        int term, index;
        command cmd;

        while (logdata.read((char*)& term, sizeof (int)) && logdata.read((char*)& index, sizeof (int))) {
            int size;
            char* buf;
            
            logdata.read((char*)& size, sizeof(int));
            
            buf = new char[size];

            logdata.read(buf, size);
            cmd.deserialize(buf, size);

            while (index >= log.size()) log.push_back(log_entry<command>());
            log[index] = log_entry<command>(cmd, term, index);

            fprintf(stderr, "restore_logdata: log.term: %d, log.index: %d, log.cmd: %d\n", term, index, cmd.value);
        }

        logdata.close();
    };
    return;
};

template<typename command>
void raft_storage<command>::persist_metadata(const int currentTerm, const int votedFor) {
    mtx.lock();
    std::ofstream metadata;
    metadata.open(file_path_metadata, std::ios::out | std::ios::app | std::ios::binary);
    if (metadata.is_open()) {
        metadata.write((char*)& currentTerm, sizeof(int));
        metadata.write((char*)& votedFor, sizeof(int));

        metadata.close();

        fprintf(stderr, "persist_metadata: currentTerm: %d, votedFor: %d\n", currentTerm, votedFor);
    };
    mtx.unlock();
    return;
};

template<typename command>
void raft_storage<command>::restore_metadata(int& currentTerm, int& votedFor) {
    std::ifstream metadata;
    metadata.open(file_path_metadata, std::ios::in);

    if (metadata.is_open()) {
        while (metadata.read((char*)& currentTerm, sizeof(int)) && metadata.read((char*)& votedFor, sizeof(int))) {};
        // metadata.seekg(sizeof (int) + sizeof (int), std::ios::end);
        // metadata.read((char*)& currentTerm, sizeof(int));
        // metadata.read((char*)& votedFor, sizeof(int));

        metadata.close();

        fprintf(stderr, "restore_metadata: currentTerm: %d, votedFor: %d\n", currentTerm, votedFor);
    };

    return;
};

#endif // raft_storage_h