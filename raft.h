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
#include <random>
#include <set>

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

    // Your code here:
    int commit_index;
    int last_applied;

    std::clock_t last_response_time;

    std::set<int> follower_id_set;

    std::vector<int> next_index;
    std::vector<int> match_index;


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

    void change_from_follower_to_candidate();
    void change_from_leader_to_follower();
    void change_from_candidate_to_follower();
    void change_from_candidate_to_leader();
    void begin_new_term(int term);
    void send_election_msg_to_all();

    int get_last_log_index();

    int get_log_term(int index);
    bool judge_match(int prev_log_index, int prev_log_term);

    int get_commit_index();
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
    last_response_time = clock();

    follower_id_set.clear();

    commit_index = 0;
    last_applied = 0;

    current_term = storage->current_term;
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
    
    // RAFT_LOG("start");
    this->background_election = new std::thread(&raft::run_background_election, this);
    this->background_ping = new std::thread(&raft::run_background_ping, this);
    this->background_commit = new std::thread(&raft::run_background_commit, this);
    this->background_apply = new std::thread(&raft::run_background_apply, this);
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::new_command(command cmd, int &term, int &index) {
    // Your code here:
    mtx.lock();
    if (role == leader) {
        log_entry<command> new_log;
        new_log.cmd = cmd;
        new_log.term = current_term;

        storage->log.push_back(new_log);
        storage->flush();

        term = current_term;
        index = get_last_log_index();

        mtx.unlock();
        // RAFT_LOG("RECEIVE NEW COMMAND");
        return true;
    }
    mtx.unlock();

    return false;
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
    // RAFT_LOG("RECEIVE REQUEST FROM %d, TERM %d", args.candidateId, args.term);
    if (args.term >= current_term) {
        if (args.term > current_term) {
            if (role == leader) {
                change_from_leader_to_follower();
            }
            if (role == candidate) {
                change_from_candidate_to_follower();
            }
            begin_new_term(args.term);
        }
        
        if (storage->vote_for == -1 || storage->vote_for == args.candidateId) {
            int last_log_index = get_last_log_index();
            int last_log_term = get_log_term(last_log_index);
            if (last_log_term < args.lastLogTerm || (last_log_term == args.lastLogTerm && last_log_index <= args.lastLogIndex)) {
                reply.voteGranted = true;
                storage->vote_for = args.candidateId;
                storage->flush();

                mtx.unlock();
                // RAFT_LOG("VOTE FOR %d", storage->vote_for);
                return 0;
            }
        }
    }
    reply.term = current_term;
    mtx.unlock();

    reply.voteGranted = false;
    return 0;
}


template<typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(int target, const request_vote_args& arg, const request_vote_reply& reply) {
    // Your code here:
    mtx.lock();
    if (reply.term > current_term) {
        if (role == leader) {
            change_from_leader_to_follower();
        }
        if (role == candidate) {
            change_from_candidate_to_follower();
        }
        begin_new_term(reply.term);
        mtx.unlock();
        return;
    }
    if (role != candidate) {
        mtx.unlock();
        return;
    }
    if (reply.voteGranted) {
        follower_id_set.insert(target);
    }
    if ((int) follower_id_set.size() > num_nodes() / 2) {
        change_from_candidate_to_leader();
        // RAFT_LOG("I'm LEADER");
    }
    mtx.unlock();
    return;
}


template<typename state_machine, typename command>
int raft<state_machine, command>::append_entries(append_entries_args<command> arg, append_entries_reply& reply) {
    // Your code here:
    mtx.lock();

    // if unknown message, ignore
    if (arg.term < current_term) {
        reply.success = false;
        reply.term = current_term;
        mtx.unlock();
        return 0;
    }

    // SPECIAL
    if (arg.leaderId == my_id) {
        reply.success = true;
        mtx.unlock();
        return 0;
    }

    // if nessary update current term
    if (current_term < arg.term) {
        if (role == leader) {
            change_from_leader_to_follower();
        }
        if (role == candidate) {
            change_from_candidate_to_follower();
        }
        begin_new_term(arg.term);
    }
    
    // PING
    if (arg.entries.empty()) {
        last_response_time = clock();
        // RAFT_LOG("PING FROM %d", arg.leaderId);
        if (role == candidate) {
            change_from_candidate_to_follower();
        }
        if (judge_match(arg.prevLogIndex, arg.prevLogTerm) && arg.prevLogIndex == get_last_log_index()) {
            if (arg.leaderCommit > commit_index) {
                commit_index = std::min(arg.leaderCommit, get_last_log_index());
                // RAFT_LOG("CURRENT COMMIT INDEX %d", commit_index);
            }
        }
        mtx.unlock();
        return 0;
    }
    // UNMATCH
    if (!judge_match(arg.prevLogIndex, arg.prevLogTerm)) {
        reply.success = false;
        reply.term = current_term;
        mtx.unlock();
        return 0;
    }
    
    // TODO: REDUCE MEMORY OCCUPY OF VECTOR
    // TODO: IMPROVE STORAGE PERFORMANCE
    // RAFT_LOG("APPEND ENTRIES");
    storage->log.erase(storage->log.begin() + arg.prevLogIndex, storage->log.end());
    storage->log.insert(storage->log.end(), arg.entries.begin(), arg.entries.end());
    
    storage->flush();
    reply.success = true;

    if (arg.leaderCommit > commit_index) {
        commit_index = std::min(arg.leaderCommit, get_last_log_index());
    }
    mtx.unlock();
    return 0;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(int target, const append_entries_args<command>& arg, const append_entries_reply& reply) {
    // Your code here:

    if (role != leader) {
        return;
    }

    // PING
    if (arg.entries.empty()) {
        return;
    }

    // OTHER
    if (!reply.success) {
        mtx.lock();
        if (reply.term > current_term) {
            if (role == leader) {
                change_from_leader_to_follower();
            }
            if (role == candidate) {
                change_from_candidate_to_follower();
            }
            begin_new_term(reply.term);
        }
        else {
            if (next_index[target] > 1) {
                --next_index[target];
            }
        }
        mtx.unlock();
        return;
    }

    mtx.lock();
    match_index[target] = arg.prevLogIndex + arg.entries.size();
    next_index[target] = match_index[target] + 1;

    commit_index = get_commit_index();
    mtx.unlock();

    // RAFT_LOG("CURRENT COMMIT INDEX %d", commit_index);
    
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

    
    while (true) {
        if (is_stopped()) return;
        // Your code here:
        mtx.lock();
        if (role == follower || role == candidate) {

            std::random_device dev;
            std::mt19937 rng(dev());
            std::uniform_real_distribution<double> d(0.2, 0.3);
            double time_out_num = d(rng);

            if (role == follower) {
                double timeout = time_out_num;
                double delta_time = (double)(clock() - last_response_time) / CLOCKS_PER_SEC;

                if (delta_time > timeout) {
                    change_from_follower_to_candidate();

                    begin_new_term(current_term + 1);
                    // RAFT_LOG("ELECTION");

                    send_election_msg_to_all();
                }
            }
            else if (role == candidate) {
                double timeout = 0.6;
                double delta_time = (double)(clock() - last_response_time) / CLOCKS_PER_SEC;
                
                if (delta_time > timeout) {
                    begin_new_term(current_term + 1);
                    // RAFT_LOG("ELECTION");

                    send_election_msg_to_all();
                }
            }
        }
        mtx.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

        mtx.lock();
        if (role == leader) {
            for (int i = 0; i < num_nodes(); ++i) {
                append_entries_args<command> args;
                if (get_last_log_index() < next_index[i]) {
                    continue;
                }

                args.term = current_term;
                args.leaderId = my_id;
                args.leaderCommit = commit_index;

                int prev_log_index = next_index[i] - 1;

                args.prevLogIndex = prev_log_index;
                args.prevLogTerm = get_log_term(prev_log_index);
                args.entries = std::vector<log_entry<command>>(storage->log.begin() + prev_log_index, storage->log.end());
                
                // SPECIAL JUDGE: WHEN THE THREAD POOL IS FULL
                // MAINLY FOR TEST PART3 UNRELIABLE NETWORK
                if (!thread_pool->addObjJob(this, &raft::send_append_entries, i, args)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        mtx.unlock();
        
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

        mtx.lock();
        while (commit_index > last_applied) {
            state->apply_log(storage->log[last_applied].cmd);
            ++last_applied;
        }
        mtx.unlock();
        
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

        mtx.lock();
        if (role == leader) {
            for (int i = 0; i < num_nodes(); ++i) {
                append_entries_args<command> args;
                args.term = current_term;
                args.leaderId = my_id;
                args.leaderCommit = commit_index;

                int prev_log_index = next_index[i] - 1;

                args.prevLogIndex = prev_log_index;
                args.prevLogTerm = get_log_term(prev_log_index);
                args.entries.clear();

                // RAFT_LOG("SEND PING TO %d", i);
                thread_pool->addObjJob(this, &raft::send_append_entries, i, args);
            }
        }
        mtx.unlock();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Change the timeout here!
    }    
    return;
}


/******************************************************************

                        Other functions

*******************************************************************/
template<typename state_machine, typename command>
void raft<state_machine, command>::change_from_leader_to_follower() {
    role = follower;
    follower_id_set.clear();
}

template<typename state_machine, typename command>
void raft<state_machine, command>::change_from_candidate_to_follower() {
    role = follower;
    follower_id_set.clear();
}

template<typename state_machine, typename command>
void raft<state_machine, command>::change_from_follower_to_candidate() {
    role = candidate;
    follower_id_set.clear();
}

template<typename state_machine, typename command>
void raft<state_machine, command>::change_from_candidate_to_leader() {
    next_index = std::vector<int>(num_nodes(), get_last_log_index() + 1);
    match_index = std::vector<int>(num_nodes(), 0);
    role = leader;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::begin_new_term(int term) {
    follower_id_set.clear();
    current_term = term;
    last_response_time = clock();

    storage->current_term = term;
    storage->vote_for = -1;
    storage->flush();
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_election_msg_to_all() {
    request_vote_args args;
    args.term = current_term;
    args.candidateId = my_id;
    args.lastLogIndex = get_last_log_index();
    args.lastLogTerm = get_log_term(args.lastLogIndex);

    for (int i = 0; i < num_nodes(); ++i) {
        // RAFT_LOG("SEND ELECT MESSAGE TO %d", i);
        if (!thread_pool->addObjJob(this, &raft::send_request_vote, i, args)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

template<typename state_machine, typename command>
int raft<state_machine, command>::get_last_log_index() {
    return storage->log.size();
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::judge_match(int prev_log_index, int prev_log_term) {
    if (get_last_log_index() < prev_log_index) {
        return false;
    }

    return get_log_term(prev_log_index) == prev_log_term;
}

template<typename state_machine, typename command>
int raft<state_machine, command>::get_log_term(int index) {
    if (index > get_last_log_index()) {
        return -1;
    }
    if (index == 0) {
        return 0;
    }
    return storage->log[index - 1].term;
}

template<typename state_machine, typename command>
int raft<state_machine, command>::get_commit_index() {
    std::vector<int> copy = match_index;
    std::sort(copy.begin(), copy.end());
    return copy[copy.size() / 2];
}

#endif // raft_h