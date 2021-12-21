#include "shard_client.h"


int shard_client::put(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    /* Store the key into primary and all other stores */
    printf("shard_client[%d] put <key, val> = <%d, %d> from tx_region[%d]\n", this->shard_id, var.key, var.value, var.tx_id);

    value_entry val(var.value);

    store[primary_replica][var.key] = val;
    
    for (int i = 0; i < (int) store.size(); i++) {
        if (i != primary_replica)
            store[i][var.key] = val;
    };
    
    return 0;
}

int shard_client::get(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    /* Get the key from the primary store */
    std::map< int, value_entry >::iterator iter = store[primary_replica].find(var.key);

    if (iter != store[primary_replica].end()) {
        r = iter->second.value;
        printf("shard_client[%d] get <key, val> = <%d, %d> from tx_region[%d]\n", this->shard_id, var.key, r, var.tx_id);
    } else {
        r = 0;
        printf("shard_client[%d] get <key, val> = <%d, ??> from tx_region[%d]\n", this->shard_id, var.key, var.tx_id);
    }
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