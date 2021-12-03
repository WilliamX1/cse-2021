#include "shard_client.h"


int shard_client::put(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::get(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::commit(chdb_protocol::commit_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::rollback(chdb_protocol::rollback_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::check_prepare_state(chdb_protocol::check_prepare_state_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::prepare(chdb_protocol::prepare_var var, int &r) {
    // TODO: Your code here
    return 0;
}

shard_client::~shard_client() {
    delete node;
}