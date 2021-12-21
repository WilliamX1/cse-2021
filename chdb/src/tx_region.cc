#include "tx_region.h"


int tx_region::put(const int key, const int val) {
    // TODO: Your code here
    printf("tx[%d] put <key, value> = <%d, %d>\n", tx_id, key, val);
    cache_[key] = val;
    return 0;
}

int tx_region::get(const int key) {
    // TODO: Your code here
    int val = 0;
    if (cache_.find(key) == cache_.end()) {
        int target_shard_client_id = this->db->dispatch(key, (int) this->db->shards.size());
        chdb_protocol::operation_var var(this->tx_id, key);
        int state = this->db->shard_id2shard(target_shard_client_id)->get(var, val);
        printf("tx[%d] get <key, value> = <%d, %d> ~ from shard_client[%d]\n", tx_id, key, val, target_shard_client_id);
    } else {
        val = cache_[key];
        printf("tx[%d] get <key, value> = <%d, %d> ~ from tx_region cache\n", tx_id, key, val);
    }
    return val;
}

int tx_region::tx_can_commit() {
    // TODO: Your code here
    int ret = chdb_protocol::prepare_ok;

    /* Check if every shard is OK */
    std::map<int, int>::iterator iter = cache_.begin();
    while (iter != cache_.end()) {
        int key = iter->first;
        int target_shard_client_id = this->db->dispatch(key, (int) this->db->shards.size());

        if (this->db->shard_id2shard(target_shard_client_id)->active == false) ret = chdb_protocol::prepare_not_ok;

        iter++;
    }
    std::string is_ok = ret == chdb_protocol::prepare_ok ? "Yes" : "No";

    printf("tx[%d] can commit ? %s\n", tx_id, is_ok.c_str());
    return ret;
}

int tx_region::tx_begin() {
    // TODO: Your code here
    printf("tx[%d] begin\n", tx_id);
    return 0;
}

int tx_region::tx_commit() {
    // TODO: Your code here
    printf("tx[%d] commit\n", tx_id);
    
    std::map<int, int>::iterator iter = cache_.begin();
    int key, val;
    while (iter != cache_.end()) {
        key = iter->first; val = iter->second;
        int target_shard_client_id = this->db->dispatch(key, (int) this->db->shards.size());
        chdb_protocol::operation_var var(this->tx_id, key, val);
        int r = 0;
        int state = this->db->shard_id2shard(target_shard_client_id)->put(var, r);
        printf("tx[%d] commit <key, value> = <%d, %d> ~ to shard_client[%d]\n", tx_id, key, val, target_shard_client_id);

        iter++;
    };

    return 0;
}

int tx_region::tx_abort() {
    // TODO: Your code here
    printf("tx[%d] abort\n", tx_id);
    return 0;
}
