#include "chdb/test/chdb_test.h"

/**
 * ***************************
 *  Part 1
 * ***************************
 * */
TEST_CASE(part1, simple_tx_1, "Simple transaction") {
    chdb store(12, CHDB_PORT);
    tx_put_and_check(&store);
}

TEST_CASE(part1, simple_tx_2, "Simple transaction") {
    chdb store(12, CHDB_PORT);
    tx_incr(&store);
}


TEST_CASE(part1, simple_tx_abort, "Simple transaction of abort") {
    chdb store(12, CHDB_PORT);
    store.set_shards_down({1, 2});

    int r;
    int write_val = 1024;
    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < 3; ++key) {
            db_client.put(key, write_val + key);
        }

        for (int key = 0; key < 3; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_not_ok, "Transaction should abort");
    }

    // Check transaction result
    {
        tx_region db_client(&store);
        for (int key = 0; key < 3; ++key) {
            r = db_client.get(key);
            ASSERT(r != write_val + key, "transaction read bad result");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }
}

TEST_CASE(part1, random_tx_abort, "Random choose shard to abort") {
    int write_val = 1024, r = 0, shard_num = 64;
    chdb store(shard_num, CHDB_PORT);
    std::set<int> deactive_shards;

    // random choose
    for (int i = 0; i < 10; ++i) {
        int num = static_cast<int>(random() % shard_num);
        deactive_shards.insert(num);
    }
    {
        store.set_shards_down(deactive_shards);
    }

    bool commit_succ = chdb_protocol::prepare_ok;
    {
        tx_region db_client(&store);
        for (int i = 0; i < 200; ++i) {
            int key = random();
            db_client.put(key, write_val + key);
            if (deactive_shards.find(key % shard_num) != deactive_shards.end())
                commit_succ = chdb_protocol::prepare_not_ok;
        }
        ASSERT(commit_succ == db_client.tx_can_commit(), "Transaction result not match");
    }
}


TEST_CASE(part1, shard_dispatch_test_target_one, "Test assigned dispatch") {
    int r;
    const int key_upper_bound = 10, shard_num = 12;
    const int write_val = 32;
    chdb store(shard_num, CHDB_PORT, test_dispatch_target_one);

    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val + key);
        }

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // Check transaction result
    {
        tx_region db_client(&store);
        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "transaction read bad result");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");


    }

    std::vector<int> expected_distri = {key_upper_bound};

    for (int idx = 1; idx < shard_num; ++idx) {
        expected_distri.push_back(0);
    }
    for (int idx = 0; idx < shard_num; ++idx) {
        int sz = store.shards[idx]->get_store().size();
        // printf("idx: %d, expected size: %d, actual size: %d\n", idx, expected_distri[idx], sz);
        ASSERT(sz == expected_distri[idx], "Shard dispatch distribution into one target bad");
    }

}

TEST_CASE(part1, shard_dispatch_test_in_static_rage, "Test assigned dispatch in static range") {
    int r;
    const int key_upper_bound = 100, shard_num = 6;
    const int write_val = 32;
    chdb store(shard_num, CHDB_PORT, test_dispatch_in_static_range);

    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val + key);
        }

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // Check transaction result
    {
        tx_region db_client(&store);
        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "transaction read bad result");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    std::vector<int> expected_distri = {10, 10, 20, 20, 40, 0};

    for (int idx = 0; idx < shard_num; ++idx) {
        int sz = store.shards[idx]->get_store().size();
        ASSERT(sz == expected_distri[idx], "Shard dispatch distribution into one target bad");
    }
}

/**
 * ***************************
 *  Part 2
 * ***************************
 * */
TEST_CASE(part2, shard_client_replica, "Test shard client replication") {
    const int key_upper_bound = 100, shard_num = 5;
    int write_val = 512, r;
    chdb store(shard_num, CHDB_PORT);
    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val + key);
        }

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }
    for (int i = 0; i < shard_num; ++i) {
        store.shards[i]->shuffle_primary_replica();
    }
    {
        tx_region db_client(&store);

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "read bad result. Check the replication in shard client!");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }
}


TEST_CASE(part2, shard_client_replica_with_put, "Test shard client shuffle replica before put") {
    const int key_upper_bound = 5, shard_num = 10;
    int write_val = random(), r;
    chdb store(shard_num, CHDB_PORT);

    // setup initial value
    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val + key);
        }

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }
    for (int i = 0; i < shard_num; ++i) {
        store.shards[i]->shuffle_primary_replica();
    }
    {
        tx_region db_client(&store);

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "read bad result.");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // shuffle replica
    for (int i = 0; i < shard_num; ++i) {
        store.shards[i]->shuffle_primary_replica();
    }
    write_val = random();
    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val << key);
        }

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val << key, "local transaction read bad result");
        }

        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    for (int i = 0; i < shard_num; ++i) {
        store.shards[i]->shuffle_primary_replica();
    }
    // put new value again
    {
        tx_region db_client(&store);

        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val << key, "read bad result. Check the replication in shard client!");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }
}

TEST_CASE(part2, raft_view_server, "Test simple view server raft group") {
    const int key_upper_bound = 20, shard_num = 5;
    int write_val = random(), r;
    chdb store(shard_num, CHDB_PORT);

    // fetch a ref of raft group
    auto group = store.vserver->raft_group;
    int leader1 = group->check_exact_one_leader();

    {
        tx_region db_client(&store);
        // dummy request
        for (int key = 0; key < key_upper_bound; ++key) {
            db_client.put(key, write_val + key);
        }
        group->disable_node(leader1);
        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "local transaction read bad result");
        }

        int leader2 = group->check_exact_one_leader();
        group->enable_node(leader1);
        group->disable_node(leader2);
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
        group->enable_node(leader2);
    }
    leader1 = group->check_exact_one_leader();
    {
        tx_region db_client(&store);
        group->disable_node(leader1);
        for (int key = 0; key < key_upper_bound; ++key) {
            r = db_client.get(key);
            ASSERT(r == write_val + key, "read bad result. Check the replication in shard client!");
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
        group->enable_node(leader1);
    }
    group->check_exact_one_leader();
}


/**
 * ***************************
 *  Part 3
 * ***************************
 * */

TEST_CASE(part3, tx_lock_test_1, "Test concurrent transaction") {
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;

    for (int i = 0; i < 16; ++i) {
        tx_list.push_back(new std::thread([&store]() {
            tx_put_and_check(&store);
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }
}

TEST_CASE(part3, tx_lock_test_2, "Test concurrent transaction") {
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;
    int key = 5, base_val = 10, thr_num = 16;
    {
        tx_region db_client(&store);
        db_client.put(key, base_val);
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // run in parallel
    for (int i = 0; i < thr_num; ++i) {
        tx_list.push_back(new std::thread([&store, &key]() {
            // contention on a single KV pair
            {
                tx_region db_client(&store);
                int r = db_client.get(key);
                // incr the value
                db_client.put(key, ++r);
                ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            }
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }

    {
        tx_region db_client(&store);
        int r = db_client.get(key);
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
        ASSERT(r == thr_num + base_val, "Incr result bad");
    }
}

TEST_CASE(part3, tx_lock_test_3, "Test concurrent transaction") {
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;
    std::vector<int> keys = {1, 5, 10, 15, 20, 25};
    int base_val = 10, thr_num = 16;
    {
        tx_region db_client(&store);
        for (auto &key: keys) {
            db_client.put(key, base_val + key);
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // run in parallel
    for (int i = 0; i < thr_num; ++i) {
        tx_list.push_back(new std::thread([&store, &keys]() {
            // contention on multiple KV pairs
            {
                tx_region db_client(&store);
                for (auto &key: keys) {
                    int r = db_client.get(key);
                    // incr the value
                    db_client.put(key, ++r);
                }
                ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            }
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }

    {
        tx_region db_client(&store);
        for (auto &key: keys) {
            int r = db_client.get(key);
            ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            ASSERT(r == thr_num + base_val + key, "Incr result bad");
        }
    }
}

inline static void asset_no_big_lock() {
#if BIG_LOCK
    ASSERT(false, "(Bonus testcase) Don't use big lock! Please disable BIG_LOCK in common.h");
#endif
}

TEST_CASE(part3, tx_lock_test_4, "(2PL)Test concurrent transaction") {
    asset_no_big_lock();
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;

    for (int i = 0; i < 16; ++i) {
        tx_list.push_back(new std::thread([&store]() {
            tx_put_and_check(&store);
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }
}

TEST_CASE(part3, tx_lock_test_5, "(2PL)Test concurrent transaction") {
    asset_no_big_lock();
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;
    int key = 5, base_val = 10, thr_num = 16;
    {
        tx_region db_client(&store);
        db_client.put(key, base_val);
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // run in parallel
    for (int i = 0; i < thr_num; ++i) {
        tx_list.push_back(new std::thread([&store, &key]() {
            // contention on a single KV pair
            {
                tx_region db_client(&store);
                int r = db_client.get(key);
                // incr the value
                db_client.put(key, ++r);
                ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            }
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }

    {
        tx_region db_client(&store);
        int r = db_client.get(key);
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
        ASSERT(r == thr_num + base_val, "Incr result bad");
    }
}

TEST_CASE(part3, tx_lock_test_6, "(2PL)Test concurrent transaction") {
    asset_no_big_lock();
    chdb store(12, CHDB_PORT);
    std::vector<std::thread *> tx_list;
    std::vector<int> keys = {1, 5, 10, 15, 20, 25};
    int base_val = 10, thr_num = 16;
    {
        tx_region db_client(&store);
        for (auto &key: keys) {
            db_client.put(key, base_val + key);
        }
        ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
    }

    // run in parallel
    for (int i = 0; i < thr_num; ++i) {
        tx_list.push_back(new std::thread([&store, &keys]() {
            // contention on multiple KV pairs
            {
                tx_region db_client(&store);
                for (auto &key: keys) {
                    int r = db_client.get(key);
                    // incr the value
                    db_client.put(key, ++r);
                }
                ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            }
        }));
    }

    for (auto &thr: tx_list) {
        if (thr) thr->join();

        delete thr;
    }

    {
        tx_region db_client(&store);
        for (auto &key: keys) {
            int r = db_client.get(key);
            ASSERT(db_client.tx_can_commit() == chdb_protocol::prepare_ok, "Transaction should commit");
            ASSERT(r == thr_num + base_val + key, "Incr result bad");
        }
    }
}


int main(int argc, char **argv) {
    unit_test_suite::instance()->run(argc, argv);
    return 0;
}