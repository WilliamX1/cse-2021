#include "ch_db.h"
#include <map>
#include <mutex>
#include <set>

/*
 * tx_region: chdb KV client which supports transaction concurrency control.
 * */
class tx_region {
public:
    tx_region(chdb *db) : db(db),
                          tx_id(db->next_tx_id()) {
        // mtx.lock();
        this->db->tx_id_mtx.lock();
        
        this->tx_begin();
    }

    ~tx_region() {
        if (this->tx_can_commit() == chdb_protocol::prepare_ok) this->tx_commit();
        else this->tx_abort();

        // mtx.unlock();
        this->db->tx_id_mtx.unlock();
    }

    /**
     * Dummy request. Only for test
     * */
    int dummy() {
        int r;
        this->db->vserver->execute(1,
                                   chdb_protocol::Dummy,
                                   chdb_protocol::operation_var{.tx_id = tx_id, .key = 1024, .value = 16},
                                   r);
        return r;
    }

    /**
     * Put one kv into the storage
     *
     * Note!: The changes in a the transaction region cannot be viewed by other regions until it commits.
     * Meanwhile, those changes can be **seen** in self region.
     * */
    int put(const int key, const int val);

    /**
     * Query one value from the storage by `key`
     * */
    int get(const int key);

    /**
     * Transaction check whether could commit.
     * Return 1 if all of the shards are ok to commit, and 0 if exists one not ok.
     * */
    int tx_can_commit();

    /*******************************************
     * Transaction part
     * ********************************************/
private:
    /* Used to cache key-value.
     * First, translate key into shard_client_id.
     * Second, use this shard_client_id as the outer layer 'key' to get corresponding map.
     * Final, store key-value into this map.
     */
    std::map<int, std::map< int, int > > cache_map_;

    /* Used to store the first old value in original shard_clients.
     * For every 'put' request, if cache has its val, then do nothing.
     * else, post a 'get' rpc_request to shard_client to get its first old value.
     */
    std::map<int, std::map< int, int > > write_map_;

    /**
     * Transaction begin
     * */
    int tx_begin();

    /**
     * Transaction commit. Sending `Commit` messages to all of the shard clients
    * */
    int tx_commit();

    /**
     * Transaction abort. Sending `Rollback` messages to all of the shard clients
     * */
    int tx_abort();

    chdb *db;
    const int tx_id;

    /* Big lock to pass test3 */
    std::mutex mtx;
};
