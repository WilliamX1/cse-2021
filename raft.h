#ifndef raft_h
#define raft_h

#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include <algorithm>
#include <thread>
#include <stdarg.h>
#include <numeric>

#include "rpc.h"
#include "raft_storage.h"
#include "raft_protocol.h"
#include "raft_state_machine.h"

template<typename state_machine, typename command>
class raft {

static_assert(std::is_base_of<raft_state_machine, state_machine>(), "state_machine must inherit from raft_state_machine");
static_assert(std::is_base_of<raft_command, command>(), "command must inherit from raft_command");


friend class thread_pool;

#define RAFT_LOG(fmt, args...) \
    do { \
        auto now = \
        std::chrono::duration_cast<std::chrono::milliseconds>(\
            std::chrono::system_clock::now().time_since_epoch()\
        ).count();\
        printf("[%ld][%s:%d][node %d term %d] " fmt "\n", now, __FILE__, __LINE__, my_id, current_term, ##args); \
    } while(0);

public:
    raft(
        rpcs* rpc_server,
        std::vector<rpcc*> rpc_clients,
        int idx, 
        raft_storage<command>* storage,
        state_machine* state    
    );
    ~raft();

    // start the raft node.
    // Please make sure all of the rpc request handlers have been registered before this method.
    void start();

    // stop the raft node. 
    // Please make sure all of the background threads are joined in this method.
    // Notice: you should check whether is server should be stopped by calling is_stopped(). 
    //         Once it returns true, you should break all of your long-running loops in the background threads.
    void stop();

    // send a new command to the raft nodes.
    // This method returns true if this raft node is the leader that successfully appends the log.
    // If this node is not the leader, returns false. 
    bool new_command(command cmd, int &term, int &index);

    // returns whether this node is the leader, you should also set the current term;
    bool is_leader(int &term);

    // save a snapshot of the state machine and compact the log.
    bool save_snapshot();

private:
    std::mutex mtx;                     // A big lock to protect the whole data structure
    ThrPool* thread_pool;
    raft_storage<command>* storage;              // To persist the raft log
    state_machine* state;  // The state machine that applies the raft log, e.g. a kv store

    rpcs* rpc_server;               // RPC server to recieve and handle the RPC requests
    std::vector<rpcc*> rpc_clients; // RPC clients of all raft nodes including this node
    int my_id;                     // The index of this node in rpc_clients, start from 0

    std::atomic_bool stopped;

    enum raft_role {
        follower,
        candidate,
        leader
    };
    raft_role role;
    int current_term;

    std::thread* background_election;
    std::thread* background_ping;
    std::thread* background_commit;
    std::thread* background_apply;

    /* some static value */
    const int timeout_heartbeat = 150;
    const int timeout_follower_election_lower = 300;
    const int timeout_follower_election_upper = 500;
    const int timeout_candidate_election_timeout = 1000;

    // Your code here:
    int votedFor; /* index correponse to term number, value corresponse to candidateId, -1 for null */
    std::vector< log_entry<command> > log; /* log entries; each entry contains command for state machine, and term when entry was received by leader */
    int commitIndex; /* index of highest log entry known to be committed, init to 0, start from 1 */

    long long election_timer; /* update it to current time whenever get a rpc request or response, init to current time */
    std::vector< bool > culculateVote; /* 0 for not response, -1 for not support, 1 for support, restore whenever server become candidate, not need to init */

private:
    // RPC handlers
    int request_vote(request_vote_args arg, request_vote_reply& reply);

    int append_entries(append_entries_args<command> arg, append_entries_reply& reply);

    int install_snapshot(install_snapshot_args arg, install_snapshot_reply& reply);

    // RPC helpers
    void send_request_vote(int target, request_vote_args arg);
    void handle_request_vote_reply(int target, const request_vote_args& arg, const request_vote_reply& reply);

    void send_append_entries(int target, append_entries_args<command> arg);
    void handle_append_entries_reply(int target, const append_entries_args<command>& arg, const append_entries_reply& reply);

    void send_install_snapshot(int target, install_snapshot_args arg);
    void handle_install_snapshot_reply(int target, const install_snapshot_args& arg, const install_snapshot_reply& reply);


private:
    bool is_stopped();
    int num_nodes() {return rpc_clients.size();}

    // background workers    
    void run_background_ping();
    void run_background_election();
    void run_background_commit();
    void run_background_apply();

    // Your code here:
    long long get_current_time();
    int get_random(int lower, int upper);

};

template<typename state_machine, typename command>
raft<state_machine, command>::raft(rpcs* server, std::vector<rpcc*> clients, int idx, raft_storage<command> *storage, state_machine *state) :
    storage(storage),
    state(state),   
    rpc_server(server),
    rpc_clients(clients),
    my_id(idx),
    stopped(false),
    role(follower),
    current_term(0),
    background_election(nullptr),
    background_ping(nullptr),
    background_commit(nullptr),
    background_apply(nullptr)
{
    thread_pool = new ThrPool(32);

    // Register the rpcs.
    rpc_server->reg(raft_rpc_opcodes::op_request_vote, this, &raft::request_vote);
    rpc_server->reg(raft_rpc_opcodes::op_append_entries, this, &raft::append_entries);
    rpc_server->reg(raft_rpc_opcodes::op_install_snapshot, this, &raft::install_snapshot);

    // Your code here: 
    // Do the initialization
    votedFor = -1;

    log = std::vector< log_entry<command> >();
    /* append a null log as the first log */
    log_entry<command> first_null_log = log_entry<command>();
    first_null_log.term = 0;
    log.push_back(first_null_log);

    commitIndex = 0;
    election_timer = get_current_time();
}

template<typename state_machine, typename command>
raft<state_machine, command>::~raft() {
    if (background_ping) {
        delete background_ping;
    }
    if (background_election) {
        delete background_election;
    }
    if (background_commit) {
        delete background_commit;
    }
    if (background_apply) {
        delete background_apply;
    }
    delete thread_pool;    
}

/******************************************************************

                        Public Interfaces

*******************************************************************/

template<typename state_machine, typename command>
void raft<state_machine, command>::stop() {
    stopped.store(true);
    background_ping->join();
    background_election->join();
    background_commit->join();
    background_apply->join();
    thread_pool->destroy();
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::is_stopped() {
    return stopped.load();
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::is_leader(int &term) {
    term = current_term;
    return role == leader;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::start() {
    // Your code here:
    
    RAFT_LOG("start");
    this->background_election = new std::thread(&raft::run_background_election, this);
    this->background_ping = new std::thread(&raft::run_background_ping, this);
    this->background_commit = new std::thread(&raft::run_background_commit, this);
    this->background_apply = new std::thread(&raft::run_background_apply, this);
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::new_command(command cmd, int &term, int &index) {
    // Your code here:

    term = current_term;
    return true;
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::save_snapshot() {
    // Your code here:
    return true;
}



/******************************************************************

                         RPC Related

*******************************************************************/
template<typename state_machine, typename command>
int raft<state_machine, command>::request_vote(request_vote_args args, request_vote_reply& reply) {
    // Your code here:

    mtx.lock();

    /* update timestamp first */
    
    /* Reply false if term < currentTerm */
    if (args.term < current_term) {
        reply.term = current_term;
        reply.voteGranted = false;
    }

    /* if votedFor for args.term is -1 or candidateId,
     * and candidate's log is at least as up-to-date as receiver's log,
     * grant vote
     */
    else if ( ( votedFor == -1 || votedFor == args.candidateId)
        && (log[commitIndex].term < args.lastLogTerm || (log[commitIndex].term == args.lastLogTerm && commitIndex <= args.lastLogIndex))) {
            reply.term = current_term;
            reply.voteGranted = true;

            votedFor = args.candidateId;
        }
    
    else {
        reply.term = current_term;
        reply.voteGranted = false;
    }

    mtx.unlock();

    return raft_rpc_status::OK;
}


template<typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(int target, const request_vote_args& arg, const request_vote_reply& reply) {
    // Your code here:

    mtx.lock();

    /* update timestamp first */
    election_timer = get_current_time();

    if (reply.term > current_term) {
        /* update current_term and become follower */
        current_term = reply.term;
        
        role = follower;
        votedFor = -1;
    }

    else {
        assert(target < (int) culculateVote.size());

        culculateVote[target] = reply.voteGranted;
    }

    mtx.unlock();

    RAFT_LOG("RPC request_vote : get from %d, result: %d", arg.candidateId, reply.voteGranted);

    return;
}


template<typename state_machine, typename command>
int raft<state_machine, command>::append_entries(append_entries_args<command> arg, append_entries_reply& reply) {
    // Your code here:
    
    mtx.lock();

    if (arg.heartbeat && arg.term >= current_term) {
        /* update timestamp first */
        election_timer = get_current_time();

        role = follower;
        current_term = arg.term;
    }

    mtx.unlock();

    RAFT_LOG("RPC append_entries");
    return 0;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(int target, const append_entries_args<command>& arg, const append_entries_reply& reply) {
    // Your code here:
    election_timer = get_current_time();
    return;
}


template<typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(install_snapshot_args args, install_snapshot_reply& reply) {
    // Your code here:
    return 0;
}


template<typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(int target, const install_snapshot_args& arg, const install_snapshot_reply& reply) {
    // Your code here:
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_request_vote(int target, request_vote_args arg) {
    request_vote_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_request_vote, arg, reply) == 0) {
        handle_request_vote_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_append_entries(int target, append_entries_args<command> arg) {
    append_entries_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_append_entries, arg, reply) == 0) {
        handle_append_entries_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_install_snapshot(int target, install_snapshot_args arg) {
    install_snapshot_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_install_snapshot, arg, reply) == 0) {
        handle_install_snapshot_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

/******************************************************************

                        Background Workers

*******************************************************************/

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_election() {
    // Check the liveness of the leader.
    // Work for followers and candidates.

    // Hints: You should record the time you received the last RPC.
    //        And in this function, you can compare the current time with it.
    //        For example:
    //        if (current_time - last_received_RPC_time > timeout) start_election();
    //        Actually, the timeout should be different between the follower (e.g. 300-500ms) and the candidate (e.g. 1s).

    srand(time(NULL));
    
    while (true) {

        RAFT_LOG("id: %d, term: %d, role: %d", my_id, current_term, role);

        if (is_stopped()) {
            RAFT_LOG("return");
            return;
        };
        // Your code here:

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        long long current_time = get_current_time();

        switch (role)
        {
        case follower:
        {
            long long timeout_follower_election = get_random(timeout_follower_election_lower, timeout_follower_election_upper);
            if (current_time - election_timer > timeout_follower_election) {
                role = candidate;

                current_term++;

                votedFor = my_id;
                culculateVote.assign(rpc_clients.size(), false);
                culculateVote[my_id] = true;

                request_vote_args args(current_term, my_id, commitIndex, log[commitIndex].term);
                
                /* update timestamp first */
                election_timer = get_current_time();
                for (int i = 0; i < (int) rpc_clients.size(); i++)
                    if (i != my_id)
                        thread_pool->addObjJob(this, &raft::send_request_vote, i, args);
            };
            break;
        }

        case candidate:
        {

            if (std::accumulate(culculateVote.begin(), culculateVote.end(), 0) > (int) rpc_clients.size() / 2) role = leader;
            else if (current_time - election_timer > timeout_candidate_election_timeout) {
                role = follower;

                /* update timestamp first */
                election_timer = get_current_time();

                votedFor = -1;
                current_term--;
            }

            break;
        }

        case leader:
        {
            append_entries_args<command> args(true, current_term); /* null entry for heartbeat */

            for (int i = 0; i < (int) rpc_clients.size(); i++)
                if (i != my_id)
                    thread_pool->addObjJob(this, &raft::send_append_entries, i, args);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_heartbeat));
            
            break;
        }
        
        default:
            break;
        }

    }    
    

    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_commit() {
    // Send logs/snapshots to the follower.
    // Only work for the leader.

    // Hints: You should check the leader's last log index and the follower's next log index.        
    
    while (true) {
        if (is_stopped()) return;
        // Your code here:

        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }    
    
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {
    // Apply committed logs the state machine
    // Work for all the nodes.

    // Hints: You should check the commit index and the apply index.
    //        Update the apply index and apply the log if commit_index > apply_index

    
    while (true) {
        if (is_stopped()) return;
        // Your code here:

        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }    
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {
    // Send empty append_entries RPC to the followers.

    // Only work for the leader.
    
    while (true) {
        if (is_stopped()) return;
        // Your code here:


        
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Change the timeout here!
    }    
    return;
}


/******************************************************************

                        Other functions

*******************************************************************/

template<typename state_machine, typename command>
long long raft<state_machine, command>::get_current_time () {
    /* reference to https://blog.csdn.net/t46414704152abc/article/details/103531447 */
    using namespace std;
    using namespace std::chrono;

    /* 获取当前时间 */
    system_clock::time_point now = system_clock::now();

    /* 距离 1970-01-01 00:00:00 的纳秒数 */
    chrono::nanoseconds d = now.time_since_epoch();

    /* 转换为毫秒数，会有精度损失 */
    chrono::milliseconds millsec = chrono::duration_cast<chrono::milliseconds>(d);

    return millsec.count();
}

template<typename state_machine, typename command>
int raft<state_machine, command>::get_random (int lower, int upper) {
    assert(upper >= lower);
    int ans = rand() % (upper - lower) + lower;
    return ans;
}

#endif // raft_h