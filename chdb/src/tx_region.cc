#include "tx_region.h"


int tx_region::put(const int key, const int val) {
    // TODO: Your code here

    printf("tx[%d] put <key, value> = <%d, %d>\n", tx_id, key, val);
    int shard_client_id = this->db->dispatch(key, (int) this->db->shards.size());

    /* Store the first value into write_map_ */
    if (this->cache_map_[shard_client_id].find(key) == this->cache_map_[shard_client_id].end())
        this->write_map_[shard_client_id][key] = get(key);
    
    this->cache_map_[shard_client_id][key] = val;

    return 0;
}

int tx_region::get(const int key) {
    // TODO: Your code here

    printf("tx[%d] get <key, value> = <%d, ?>\n", tx_id, key);

    int ret = 0;

    int shard_client_id = this->db->dispatch(key, (int) this->db->shards.size());
    if (this->cache_map_[shard_client_id].find(key) == this->cache_map_[shard_client_id].end()) {
        chdb_protocol::operation_var var(this->tx_id, key);

        shard_client* shard = this->db->shard_id2shard(shard_client_id);

        shard->node->call(shard->node->bind_remote_node(shard->node->port()), chdb_protocol::Get, var, ret);
        
        printf("tx[%d] get <key, value> = <%d, %d> ~ from shard_client[%d]\n", tx_id, key, ret, shard_client_id);
    } else {
        ret = this->cache_map_[shard_client_id][key];
        printf("tx[%d] get <key, value> = <%d, %d> ~ from tx_region cache\n", tx_id, key, ret);
    }
    
    return ret;
}

int tx_region::tx_can_commit() {
    // TODO: Your code here

    int ret = chdb_protocol::prepare_ok;

    /* Check if every shard is OK */
    std::map<int, std::map<int, int> >::iterator shard_iter = this->write_map_.begin();

    while (shard_iter != this->write_map_.end()) {
        int shard_client_id = shard_iter->first;
        std::map<int, int> write_set = shard_iter->second;

        chdb_protocol::prepare_var prepare_var(this->tx_id, write_set);
        int r;

        shard_client* shard = this->db->shard_id2shard(shard_client_id);
        shard->node->call(shard->node->bind_remote_node(shard->node->port()), chdb_protocol::Prepare, prepare_var, r);

        if (r == chdb_protocol::prepare_not_ok) {
            ret = chdb_protocol::prepare_not_ok;
            break;
        };

        chdb_protocol::check_prepare_state_var check_prepare_state_var(this->tx_id);
        shard->node->call(shard->node->bind_remote_node(shard->node->port()), chdb_protocol::CheckPrepareState, check_prepare_state_var, r);

        if (r == chdb_protocol::prepare_not_ok) {
            ret = chdb_protocol::prepare_not_ok;
            break;
        };

        shard_iter++;
    };

    /* Output can commit or not to screen */
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

    std::map<int, std::map<int, int> >::iterator shard_iter = this->cache_map_.begin();

    while (shard_iter != this->cache_map_.end()) {
        int shard_client_id = shard_iter->first;
        std::map<int, int> write_set = shard_iter->second;

        chdb_protocol::commit_var var(this->tx_id, write_set);
        
        int r;
        shard_client* shard = this->db->shard_id2shard(shard_client_id);
        shard->node->call(shard->node->bind_remote_node(shard->node->port()), chdb_protocol::Commit, var, r);

        shard_iter++;
    }

    return 0;
}

int tx_region::tx_abort() {
    // TODO: Your code here
    printf("tx[%d] abort\n", tx_id);
    return 0;
}
