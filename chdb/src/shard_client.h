#include "common.h"
#include "protocol.h"
#include "chdb_state_machine.h"

class value_entry {
public:
    value_entry() {}

    value_entry(const value_entry &entry) : value(entry.value) {}

    value_entry(int value) : value(value) {};
    
    int value;
};

/**
 * Storage layer for each shard. Support fault tolerance.
 * */
class shard_client {
public:
    shard_client(const int shard_id, const int port) : active(true),
                                                       shard_id(shard_id),
                                                       node(new rpc_node(port)) {
        this->store.resize(this->replica_num);
        // reg rpc handlers. You may add more handlers if necessary.
        this->node->reg(chdb_protocol::Dummy, this, &shard_client::dummy);
        this->node->reg(chdb_protocol::Put, this, &shard_client::put);
        this->node->reg(chdb_protocol::Get, this, &shard_client::get);
        // transaction related
        this->node->reg(chdb_protocol::Prepare, this, &shard_client::prepare);
        this->node->reg(chdb_protocol::Commit, this, &shard_client::commit);
        this->node->reg(chdb_protocol::Rollback, this, &shard_client::rollback);
        this->node->reg(chdb_protocol::CheckPrepareState, this, &shard_client::check_prepare_state);
    }

    ~shard_client();

    void bind_view_server(const int server_port) {
        this->node->bind_remote_node(server_port);
        this->view_server_port = server_port;
    }

    int dummy(chdb_protocol::operation_var var, int &r) {
        printf("Receive dummy Request! tx id:%d\n", var.tx_id);
        r = var.tx_id;
        return 0;
    }

    int put(chdb_protocol::operation_var var, int &r);

    int get(chdb_protocol::operation_var var, int &r);

    int prepare(chdb_protocol::prepare_var var, int &r);

    int commit(chdb_protocol::commit_var var, int &r);

    /**
     * Execute rollback according to `undo_logs`
     * */
    int rollback(chdb_protocol::rollback_var var, int &r);

    int check_prepare_state(chdb_protocol::check_prepare_state_var var, int &r);

    void set_active(bool active) {
        this->active = active;
    }

    /**
     * Random pick a new replica for this shard client
     * Only used in testcase
     * */
    int shuffle_primary_replica() {
        int next = this->primary_replica;
        while (next == this->primary_replica) {
            next = random() % this->store.size();
        }
        this->primary_replica = next;
        return next;
    }

    std::map<int, value_entry> &get_store() {
        return this->store[primary_replica];
    }

    int shard_id;
    int view_server_port;
    bool active;
    rpc_node *node;
    std::vector< std::map< int, value_entry> > store;
    int primary_replica = 0;
    int replica_num = 5;

};