#include "shard_client.h"


int shard_client::put(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    mtx.lock();
    /* Store the key into primary and all other stores */
    printf("shard_client[%d] put <key, val> = <%d, %d> from tx_region[%d]\n", this->shard_id, var.key, var.value, var.tx_id);

    value_entry val(var.value);    
    for (int i = 0; i < (int) store.size(); i++)
        store[i][var.key] = val;
    mtx.unlock();
    return 0;
}

int shard_client::get(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    mtx.lock();
    /* Get the key from the primary store */
    std::map< int, value_entry >::iterator iter = store[primary_replica].find(var.key);

    if (iter != store[primary_replica].end()) {
        r = iter->second.value;
        printf("shard_client[%d] get <key, val> = <%d, %d> from tx_region[%d]\n", this->shard_id, var.key, r, var.tx_id);
    } else {
        r = 0;
        printf("shard_client[%d] get <key, val> = <%d, ??> from tx_region[%d]\n", this->shard_id, var.key, var.tx_id);
    }
    mtx.unlock();
    return 0;
}

int shard_client::commit(chdb_protocol::commit_var var, int &r) {
    // TODO: Your code here

    mtx.lock();

    printf("shard_client[%d] start commit... from tx[%d]\n", this->shard_id, var.tx_id);

    std::map<int, int>::iterator iter = var.write_map_.begin();
    int key, value;
    while (iter != var.write_map_.end()) {
        key = iter->first; value = iter->second;
        // printf("\t");
        // put(chdb_protocol::operation_var(var.tx_id, key, val), r);
        value_entry val(value);    
        for (int i = 0; i < (int) store.size(); i++)
            store[i][key] = val;

        iter++;
    };

    printf("shard_client[%d] commit successfully from tx[%d]\n", this->shard_id, var.tx_id);
    
    mtx.unlock();
    return 0;
}

int shard_client::rollback(chdb_protocol::rollback_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::check_prepare_state(chdb_protocol::check_prepare_state_var var, int &r) {
    // TODO: Your code here
    /* Return current active directly */
    printf("WARNING: Do Not Understand This Method\n");
    r = this->active;
    return 0;
}

int shard_client::prepare(chdb_protocol::prepare_var var, int &r) {
    // TODO: Your code here

    mtx.lock();

    printf("shard_client[%d] prepare check... from tx[%d]\n", this->shard_id, var.tx_id);

    r = chdb_protocol::prepare_ok;

    int key, val;
    std::map<int, int>::iterator iter = var.write_map_.begin();
    while (iter != var.write_map_.end()) {
        key = iter->first; val = iter->second;
        if (store[primary_replica].find(key) != store[primary_replica].end()
            && store[primary_replica][key].value != val) {
            printf("\tkey: %d, first old val: %d, cur val: %d\n", key, val, store[primary_replica][key].value);
            r = chdb_protocol::prepare_not_ok;
            break;
        };
        iter++;
    };

    std::string str = r == chdb_protocol::prepare_ok ? "Yes" : "No";
    printf("shard_client[%d] prepare %s from tx[%d]\n", this->shard_id, str.c_str(), var.tx_id);

    mtx.unlock();

    return 0;
}

shard_client::~shard_client() {
    delete node;
}