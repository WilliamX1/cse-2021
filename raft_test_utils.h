#ifndef test_utils_h
#define test_utils_h

#include <set>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>  
#include <dirent.h>
#include <sys/times.h>

#include "rpc.h"
#include "raft.h"


/******************************************************************

                       For Test Framework

*******************************************************************/
#define ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << ":" << __LINE__ << " msg: " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)


#define TEST_CASE(part, name, msg) \
class test_case_##part##_##name##_ut : public unit_test_case {\
public:\
    test_case_##part##_##name##_ut(const char* _msg) {this->message = _msg;}\
    virtual void body() override;\
};\
static unit_test_case* part##_##name = unit_test_suite::instance()->register_test_case(#part, #name, new test_case_##part##_##name##_ut(msg));\
void test_case_##part##_##name##_ut::body()

class unit_test_case {
public:
    virtual void body() = 0;
    const char* message;
};

class unit_test_suite {
public:
    static unit_test_suite* instance();
    unit_test_case* register_test_case(const char* part, const char* name, unit_test_case* obj);
    bool run_case(const std::string &part, const std::string &name, unit_test_case* obj);
    bool run_part_case(const std::string &part, const std::string &name);
    bool run_all();
    bool run(int argc, char** argv);
private:
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, unit_test_case*>>>> cases; // (part, name) -> case
    bool is_counting;
    int count;
    int passed;

};

void mssleep(int ms);

int remove_directory(const char *path);
/******************************************************************

                         For Raft Test

*******************************************************************/

std::vector<rpcs*> create_random_rpc_servers(int num);

std::vector<rpcc*> create_rpc_clients(const std::vector<rpcs*> &servers);

// std::vector<raft<list_state_machine, list_command>*> create_raft_nodes(int num);

class list_command : public raft_command {
public:
    list_command(){}
    list_command(const list_command &cmd) {value=cmd.value;}
    list_command(int v): value(v){}
    virtual ~list_command() {}

    virtual int size() const override {return 4;}

    virtual void serialize(char* buf, int sz) const override {
        if (sz != size()) return;
        buf[0] = (value >> 24) & 0xff;
        buf[1] = (value >> 16) & 0xff;
        buf[2] = (value >> 8) & 0xff;
        buf[3] = value & 0xff;
    }

    virtual void deserialize(const char* buf, int sz) override {
        if (sz != size()) return;
        value = (buf[0] & 0xff) << 24;
        value |= (buf[1] & 0xff) << 16;
        value |= (buf[2] & 0xff) << 8;
        value |= buf[3] & 0xff;
    }

    int value;
};

marshall& operator<<(marshall &m, const list_command& cmd);

unmarshall& operator>>(unmarshall &u, list_command& cmd);

class list_state_machine : public raft_state_machine {
public:
    list_state_machine();
    virtual ~list_state_machine() {}
    virtual std::vector<char> snapshot() override;
    
    virtual void apply_log(raft_command &cmd) override;

    virtual void apply_snapshot(const std::vector<char>&) override;

    std::mutex mtx;

    std::vector<int> store;
    int num_append_logs;
};

template<typename state_machine, typename command>
class raft_group {
public:
    // typedef raft<list_state_machine, list_command> raft<state_machine, command>;

    raft_group(int num, const char *storage_dir = "raft_temp");
    ~raft_group();

    int check_exact_one_leader();
    void check_no_leader();

    void set_reliable(bool value);

    void disable_node(int i);

    void enable_node(int i);

    int check_same_term();

    int num_committed(int log_idx);
    int get_committed_value(int log_idx);

    int append_new_command(int value, int num_committed_server);

    int wait_commit(int index, int num_committed_server, int start_term);

    int rpc_count(int node);

    int restart(int node);

    std::vector<raft<state_machine, command>*> nodes;
    std::vector<rpcs*> servers;
    std::vector<std::vector<rpcc*>> clients;
    std::vector<raft_storage<command>*> storages;
    std::vector<state_machine*> states;
};

template<typename state_machine, typename command>
raft_group<state_machine, command>::raft_group(int num, const char* storage_dir) {
    nodes.resize(num, nullptr);
    servers = create_random_rpc_servers(num);
    clients.resize(num);
    states.resize(num);
    storages.resize(num);
    remove_directory(storage_dir);
    // ASSERT(ret == 0 || , "cannot rmdir " << ret);
    ASSERT(mkdir(storage_dir, 0777) >= 0, "cannot create dir " << std::string(storage_dir));
    for (int i = 0; i < num; i++) {
        std::string dir_name(storage_dir);
        dir_name = dir_name + "/raft_storage_" + std::to_string(i);
        ASSERT(mkdir(dir_name.c_str(), 0777) >= 0, "cannot create dir " << std::string(storage_dir));
        raft_storage<command> *storage = new raft_storage<command>(dir_name);
        state_machine *state = new state_machine();
        auto client = create_rpc_clients(servers);
        raft<state_machine, command>* node = new raft<state_machine, command>(servers[i], client, i, storage, state);
        nodes[i] = node;
        clients[i] = client;
        states[i] = state;
        storages[i] = storage;
    }
    for (int i = 0; i < num; i++)
        nodes[i]->start();
}

template<typename state_machine, typename command>
raft_group<state_machine, command>::~raft_group() {
    set_reliable(true);
    for (size_t i = 0; i < nodes.size(); i++) {
        raft<state_machine, command> *node = nodes[i];
        // disbale_node(i);
        node->stop();
        for (size_t j = 0; j < nodes.size(); j++) {
            clients[i][j]->cancel();
            delete clients[i][j];
        }
        delete servers[i];
        delete node;
        delete states[i];
        delete storages[i];
    }
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::check_exact_one_leader() {
    int num_checks = 10;
    int num_nodes = nodes.size();
    for (int i = 0; i < num_checks; i++) {
        std::map<int, int> term_leaders;
        for (int j = 0; j < num_nodes; j++) {
            raft<state_machine, command> *node = nodes[j];
            if (!servers[j]->reachable()) continue;
            int term = -1;
            bool is_leader = node->is_leader(term);
            if (is_leader) {
                ASSERT(term > 0, "term " << term << " should not have a leader."); 
                ASSERT(term_leaders.find(term) == term_leaders.end(), "term " << term << " has more than one leader.");
                term_leaders[term] = j;
            }
        }
        if (term_leaders.size() > 0) {
            auto last_term = term_leaders.rbegin();
            return last_term->second; // return the leader index
        }
        // sleep a while, in case the election is not successful.
        mssleep(500 + (random() % 10) * 30);
    }
    ASSERT(0, "There is no leader"); // no leader
    return -1;
}

template<typename state_machine, typename command>
void raft_group<state_machine, command>::check_no_leader() {
   int num_nodes = nodes.size();
   for (int j = 0; j < num_nodes; j++) {
        raft<state_machine, command> *node = nodes[j];
        rpcs *server = servers[j];
        if (server->reachable()) {
            int term = -1;
            ASSERT(!node->is_leader(term), "Node " << j << " is leader, which is unexpected.");
        }
    }
    return;
}


template<typename state_machine, typename command>
int raft_group<state_machine, command>::check_same_term() {
    int current_term = -1;
    int num_nodes = nodes.size();
    for (int i = 0; i < num_nodes; i++) {
        int term = -1;
        raft<state_machine, command> *node = nodes[i];
        node->is_leader(term); // get the current term
        ASSERT(term >= 0, "invalid term: " << term);
        if (current_term == -1) current_term = term;
        ASSERT(current_term == term, "inconsistent term: " << current_term << ", " << term);
    }
    return current_term;
}

template<typename state_machine, typename command>
void raft_group<state_machine, command>::disable_node(int i) {
    rpcs* server = servers[i];
    std::vector<rpcc*> &client = clients[i];
    server->set_reachable(false);
    for (auto c : client) 
        c->set_reachable(false);
}

template<typename state_machine, typename command>
void raft_group<state_machine, command>::enable_node(int i) {
    rpcs* server = servers[i];
    std::vector<rpcc*> &client = clients[i];
    server->set_reachable(true);
    for (auto c : client)
        c->set_reachable(true);
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::num_committed(int log_idx) {
    int cnt = 0;    
    int old_value = 0;
    for (size_t i = 0; i < nodes.size(); i++) {
        list_state_machine *state = states[i];
        bool has_log;
        int log_value;
        {
            std::unique_lock<std::mutex> lock(state->mtx);
            // fprintf(stderr, "apply_size: %d, log_idx: %d\n", (int)state->store.size(), log_idx);
            if ((int)state->store.size() > log_idx) {
                log_value = state->store[log_idx];
                has_log = true;
            } else {
                has_log = false;
            }
        }
        if (has_log) {
            fprintf(stderr, "haslog: id: %d log_idx: %d\n", (int) i, log_idx);
            cnt ++;
            if (cnt == 1) {
                old_value = log_value;
            } else {
                ASSERT(old_value == log_value, "inconsistent log value: (" << log_value << ", " << old_value << ") at idx " << log_idx);
            }
        }
    }
    return cnt;
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::get_committed_value(int log_idx) {
    for (size_t i = 0; i < nodes.size(); i++) {
        list_state_machine *state = states[i];
        int log_value;
        {
            std::unique_lock<std::mutex> lock(state->mtx);
            if ((int)state->store.size() > log_idx) {
                log_value = state->store[log_idx];
                return log_value;
            }
        }
    }    
    ASSERT(false, "log " << log_idx << " is not committed." );
    return -1;
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::append_new_command(int value, int expected_servers) {
    list_command cmd(value);
    auto start = std::chrono::system_clock::now();
    int leader_idx = 0;
    while (std::chrono::system_clock::now() < start + std::chrono::seconds(10)) {
        int log_idx = -1;
        for (size_t i = 0; i < nodes.size(); i++) {
            leader_idx = (leader_idx + 1) % nodes.size();
            // FIXME: lock?
            if (!servers[leader_idx]->reachable()) continue;

            int temp_idx, temp_term;
            bool is_leader = nodes[leader_idx]->new_command(cmd, temp_term, temp_idx);
            if (is_leader) {
                log_idx = temp_idx;
                break;
            }
        }
        if (log_idx != -1) {
            auto check_start = std::chrono::system_clock::now();
            while (std::chrono::system_clock::now() < check_start + std::chrono::seconds(2)) {
                int committed_server = num_committed(log_idx);
                if (committed_server >= expected_servers) {
                    // The log is committed!
                    int commited_value = get_committed_value(log_idx);
                    if (commited_value == value)
                        return log_idx; // and the log is what we want!
                }
                mssleep(20);
            }
        } else {
            // no leader 
            mssleep(50);
        }
    }
    ASSERT(0, "Cannot make agreement!");
    return -1;
}


template<typename state_machine, typename command>
int raft_group<state_machine, command>::wait_commit(int index, int num_committed_server, int start_term) {
    int sleep_for = 10; // ms
    for (int iters = 0; iters < 30; iters++) {
        int nc = num_committed(index);
        if (nc >= num_committed_server) break;
        mssleep(sleep_for);
        if (sleep_for < 1000) sleep_for *=2;
        if (start_term > -1) {
            for (auto node : nodes) {
                int current_term;
                node->is_leader(current_term);
                if (current_term > start_term) {
                    // someone has moved on
 					// can no longer guarantee that we'll "win"
                    return -1; 
                }
            }
        }
    }
    int nc = num_committed(index);
    ASSERT(nc >= num_committed_server, "only " << nc << " decided for index " << index << "; wanted " << num_committed_server);
    return get_committed_value(index);
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::rpc_count(int node) {
    int sum = 0;
    if (node == -1) {        
        for (auto &cl : clients) {
            for (auto &cc : cl) {
                sum += cc->count();
            }
        }
    } else {
        for (auto &cc : clients[node]) {
            sum += cc->count();
        }
    }
    return sum;
}

template<typename state_machine, typename command>
int raft_group<state_machine, command>::restart(int node) {
    // std::cout << "restart " << node << std::endl;
    nodes[node]->stop();
    disable_node(node);
    servers[node]->unreg_all();
    delete nodes[node];
    delete states[node];
    states[node] = new state_machine();
    raft_storage<command> *storage = new raft_storage<command>(std::string("raft_temp/raft_storage_") + std::to_string(node));
    // recreate clients
    for (auto &cl : clients[node])
        delete cl;
    servers[node]->set_reachable(true);
    clients[node] = create_rpc_clients(servers);
    
    nodes[node] = new raft<state_machine, command>(servers[node], clients[node], node, storage, states[node]);
    // disable_node(node);
    nodes[node]->start();
    return 0;
}

template<typename state_machine, typename command>
void raft_group<state_machine, command>::set_reliable(bool value) {
    for (auto server : servers) 
        server->set_reliable(value);
}

#endif // test_utils_h
