/*
 * The test cases in the first three parts are ported from 6.824 raft lab.
 */


#include "raft_test_utils.h"

typedef raft_group<list_state_machine, list_command> list_raft_group;

TEST_CASE(part1, leader_election, "Initial election")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    
    // 1. check whether there is exact one leader in the group
    mssleep(300); // sleep 300 ms
    group->check_exact_one_leader();

    // 2. check whether every one has the same term
    mssleep(2000); // sleep 2s
    int term1 = group->check_same_term();

    // 3. wait for a few seconds and check whether the term is not changed.
    mssleep(2000); // sleep 2s
    int term2 = group->check_same_term();

    ASSERT(term1 == term2, "inconsistent term: " << term1 << ", " << term2);
    group->check_exact_one_leader();
    delete group;
}

TEST_CASE(part1, re_election, "Election after network failure")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    
    // 1. check whether there is exact one leader in the group
    mssleep(300); // sleep 300ms
    int leader1 = group->check_exact_one_leader();

    // 2. kill the leader
    group->disable_node(leader1);
    mssleep(1000); // sleep 1s
    int leader2 = group->check_exact_one_leader();
    
    ASSERT(leader1 != leader2, "node " << leader2 << "is killed, which should not be the new leader!");

    // 3. kill the second leader
    group->disable_node(leader2);
    mssleep(1000); // sleep 1s
    int leader3 = group->check_exact_one_leader();
    ASSERT(leader3 != leader1 && leader3 != leader2, "node " << leader3 << "is killed, which should not be the new leader!");

    // 4. kill the third leader
    group->disable_node(leader3);
    mssleep(1000); // sleep 1s
    
    // 5. now, there are only 2 nodes left with no leader.
    group->check_no_leader();

    // 6. resume a node
    group->enable_node(leader1);
    mssleep(1000);
    group->check_exact_one_leader();

    delete group;   
}

TEST_CASE(part2, basic_agree, "Basic Agreement")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(3);
    mssleep(300);
    int iters = 3;
    for (int i = 1; i < 1 + iters; i++) {
        int num_commited = group->num_committed(i);
        ASSERT(num_commited == 0, "The log " << i << " should not be committed!");

        int log_idx = group->append_new_command(i * 100, num_nodes);
        ASSERT(log_idx == i, "got index " << log_idx << ", but expect " << i);
    }

    delete group;
}

TEST_CASE(part2, fail_agree, "Fail Agreement")
{

    int num_nodes = 3;
    list_raft_group* group = new list_raft_group(3);

    group->append_new_command(101, num_nodes);
    int leader = group->check_exact_one_leader();
    group->disable_node(leader);

    group->append_new_command(102, num_nodes - 1);
    group->append_new_command(103, num_nodes - 1);
    mssleep(1000);
    group->append_new_command(104, num_nodes - 1);
    group->append_new_command(105, num_nodes - 1);

    group->enable_node(leader);

    group->append_new_command(106, num_nodes);
    mssleep(1000);
    group->append_new_command(107, num_nodes);

    delete group;
}

TEST_CASE(part2, fail_no_agree, "Fail No Agreement")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    mssleep(300);
    group->append_new_command(10, num_nodes);
    
    // 3 of 5 followers disconnect
    int leader = group->check_exact_one_leader();
    group->disable_node((leader+1)%num_nodes);
    group->disable_node((leader+2)%num_nodes);
    group->disable_node((leader+3)%num_nodes);

    int temp_term, temp_index;
    int is_leader = group->nodes[leader]->new_command(list_command(20), temp_term, temp_index);
    ASSERT(is_leader, "node " << leader << " is leader, but it rejects the command.");
    ASSERT(temp_index == 2, "expected index 2, got " << temp_index);

    mssleep(2000);

    int num_committed = group->num_committed(temp_index);
    ASSERT(num_committed == 0, "There is no majority, but index " << temp_index << " is committed");

    // repair
    group->enable_node((leader+1)%num_nodes);
    group->enable_node((leader+2)%num_nodes);
    group->enable_node((leader+3)%num_nodes);

    int leader2 = group->check_exact_one_leader();
    is_leader = group->nodes[leader2]->new_command(list_command(30), temp_term, temp_index);
    ASSERT(is_leader, "leader2 reject the new command");
    ASSERT(temp_index == 2 || temp_index == 3, "unexpected index " << temp_index);

    group->append_new_command(1000, num_nodes);

    delete group;
}

std::ostream& operator<< (std::ostream& out, const std::vector<int>& vec) {
    out << "[";
    for (size_t i = 0; i < vec.size(); i++) 
        out << vec[i] << (i == vec.size() - 1? ']' : ',');
    return out;
}

TEST_CASE(part2, concurrent_start, "Concurrent starts")
{
    int num_nodes = 3;
    list_raft_group* group = new list_raft_group(num_nodes);
    bool success;
    for (int tries = 0; tries < 5; tries++) {
        if (tries > 0) {
            mssleep(3000); // give solution some time to settle
        }
        int leader = group->check_exact_one_leader();
        int term, index;
        if (!group->nodes[leader]->new_command(list_command(1), term, index)) {
            continue; // leader moved on really quickly
        }
        
        int iters = 5;
        std::mutex mtx; // to protect the vector
        std::vector<int> indices;
        std::vector<std::thread*> threads;
        bool failed = false;
        std::vector<int> values;

        for (int i = 0; i < iters; i++) {
            std::thread *worker = new std::thread([=](int it, std::mutex *mtx, std::vector<int> *indices) {
                int temp_term, temp_index;
                bool is_leader = group->nodes[leader]->new_command(list_command(100 + it), temp_term, temp_index);
                if (!is_leader) return;
                if (term != temp_term) return;
                mtx->lock();
                indices->push_back(temp_index);
                mtx->unlock();
            }, i, &mtx, &indices);
            threads.push_back(worker);
        }

        for (int i = 0; i < iters; i++) {
            threads[i]->join();
            delete threads[i];
        }

        for (int i = 0; i < num_nodes; i++) {
            int temp_term;
            group->nodes[i]->is_leader(temp_term);
            if (temp_term != term) goto loop_end; // term changed -- can't expect low RPC counts
        }

        for (auto index : indices) {
            int res = group->wait_commit(index, num_nodes, term);
            if (res == -1) {
                // nodes have moved on to later terms
                failed = true;
                break;
            }
            values.push_back(res);
        }

        if (failed) continue;
        for (int i = 0; i < iters; i++) {
            int ans = i + 100;
            bool find = false;
            for (auto value : values) {
                if (value == ans) find = true;
            }
            ASSERT(find, "cmd " << ans << " missing in " << values);
        }

        success = true;
        break;
        loop_end: continue;
    }
    ASSERT(success, "term change too often");
    delete group;
}

TEST_CASE(part2, rejoin, "Rejoin of partitioned leader")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    
    group->append_new_command(101, num_nodes);
    int leader1 = group->check_exact_one_leader();

    // leader network failure
    group->disable_node(leader1);

    // make old leader try to agree on some entries
    int temp_term, temp_index;
    group->nodes[leader1]->new_command(list_command(102), temp_term, temp_index);
    group->nodes[leader1]->new_command(list_command(103), temp_term, temp_index);
    group->nodes[leader1]->new_command(list_command(104), temp_term, temp_index);

    // new leader commits, also for index=2
    int leader2 = group->check_exact_one_leader();
    group->append_new_command(103, 2);

    // new leader network failure
    group->disable_node(leader2);

	// old leader connected again
    group->enable_node(leader1);

    group->append_new_command(104, 2);

	// all together now
	group->enable_node(leader2);

    group->append_new_command(105, num_nodes);

    delete group;
}

TEST_CASE(part2, backup, "Leader backs up quickly over incorrect follower logs")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    int value = 0;

    group->append_new_command(value++, num_nodes);

    // put leader and one follower in a partition
    int leader1 = group->check_exact_one_leader();
    group->disable_node((leader1 + 2) % num_nodes);
    group->disable_node((leader1 + 3) % num_nodes);
    group->disable_node((leader1 + 4) % num_nodes);

	// submit lots of commands that won't commit
    int temp_term, temp_index;
    for (int i = 0; i < 50; i++)
        group->nodes[leader1]->new_command(list_command(value++), temp_term, temp_index);
    

    mssleep(500);

    group->disable_node(leader1);
    group->disable_node((leader1 + 1) % num_nodes);

    // allow other partition to recover
    group->enable_node((leader1 + 2) % num_nodes);
    group->enable_node((leader1 + 3) % num_nodes);
    group->enable_node((leader1 + 4) % num_nodes);
    
	// lots of successful commands to new group.
    for (int i = 0; i < 50; i++) 
        group->append_new_command(value++, 3);
    

	// now another partitioned leader and one follower
    int leader2 = group->check_exact_one_leader();
    int other = (leader1 + 2) % num_nodes;
    if (other == leader2) other = (leader2 + 1) % num_nodes;

    group->disable_node(other);

	// lots more commands that won't commit
    for (int i = 0; i < 50; i++)
        group->nodes[leader2]->new_command(list_command(value++), temp_term, temp_index);

    mssleep(500);

	// bring original leader back to life,
    for (int i = 0; i < num_nodes; i++)
        group->disable_node(i);
    group->enable_node((leader1 + 0) % num_nodes);
    group->enable_node((leader1 + 1) % num_nodes);
    group->enable_node(other);

	// lots of successful commands to new group.
    for (int i = 0; i < 50; i++) 
        group->append_new_command(value++, 3);
    

	// now everyone
    for (int i = 0; i < num_nodes; i++)
        group->enable_node(i);

    group->append_new_command(value++, num_nodes);
    delete group;
}

TEST_CASE(part2, rpc_count, "RPC counts aren't too high")
{
    int num_nodes = 3;
    int value = 1;
    list_raft_group *group = new list_raft_group(num_nodes);

    int leader = group->check_exact_one_leader();

    int total1 = group->rpc_count(-1);

    ASSERT(total1 <= 30 && total1 >= 1, "too many or few RPCs (" << total1 << ") to elect initial leader");
    
    bool success = false;
    int total2; bool failed;
    for (int tries = 0; tries < 5; tries++) {
        if (tries > 0)
            mssleep(3000); // give solution some time to settle
        leader = group->check_exact_one_leader();
        total1 = group->rpc_count(-1);

        int iters = 10;
        int term, index;
        int is_leader = group->nodes[leader]->new_command(list_command(1024), term, index);
        if (!is_leader) continue;


        for (int i = 1; i < iters + 2; i++) {
            int term1, index1;
            is_leader = group->nodes[leader]->new_command(list_command(value), term1, index1);
            if (!is_leader) goto loop_end;
            if (term1 != term) goto loop_end;
            value++;
            ASSERT(index+i == index1, "wrong commit index " << index1 << ", expected " << index+i);
        }
        for (int i = 1; i < iters + 1; i++) {
            int res = group->wait_commit(index+i, num_nodes, term);
            if (res == -1) goto loop_end;
            ASSERT(res == i, "wrong value " << res << " committed for index " << index+i << "; expected " << i);
        }

        failed = false;
        for (int j = 0; j < num_nodes; j++) {
            int temp_term;
            group->nodes[j]->is_leader(temp_term);
            if (temp_term != term) {
                failed = true;
            }
        }
        if (failed) goto loop_end;

        total2 = group->rpc_count(-1);
        ASSERT(total2 - total1 <= (iters + 1 + 3)*3, "too many RPCs (" << total2-total1 << ") for " << iters << " entries");

        success = true;
        break;
        loop_end: continue;
    }

    ASSERT(success, "term changed too often");
    mssleep(1000);

    int total3 = group->rpc_count(-1);
    ASSERT(total3 - total2 <= 3 * 20, "too many RPCs (" << total3-total2 << ") for 1 second of idleness");
    delete group;
}


TEST_CASE(part3, persist1, "Basic persistence")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    mssleep(100);
    group->append_new_command(11, num_nodes);

    for (int i = 0; i < num_nodes; i++) {
        group->restart(i);
    }
    for (int i = 0; i < num_nodes; i++) {
        group->disable_node(i);
        group->enable_node(i);
    }

    group->append_new_command(12, num_nodes);

    int leader1 = group->check_exact_one_leader();
    group->disable_node(leader1);
    group->restart(leader1);
    group->enable_node(leader1);
    
    group->append_new_command(13, num_nodes);

    int leader2 = group->check_exact_one_leader();
    group->disable_node(leader2);
    group->append_new_command(14, num_nodes - 1);
    group->restart(leader2);
    group->enable_node(leader2);

    group->wait_commit(4, num_nodes, -1); // wait for leader2 to join before killing i3

    int i3 = (group->check_exact_one_leader() + 1) % num_nodes;
    group->disable_node(i3);
    group->append_new_command(15, num_nodes - 1);
    group->restart(i3);
    group->enable_node(i3);

    group->append_new_command(16, num_nodes);
	
    delete group;
}

TEST_CASE(part3, persist2, "More persistence")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    
    int index = 1;
    for (int iters = 0; iters < 5; iters++) {
        group->append_new_command(10 + index, num_nodes);
        index++;

        int leader1 = group->check_exact_one_leader();

        group->disable_node((leader1 + 1) % num_nodes);
        group->disable_node((leader1 + 2) % num_nodes);

        group->append_new_command(10 + index, num_nodes - 2);
        index++;

        
        group->disable_node((leader1 + 0) % num_nodes);
        group->disable_node((leader1 + 3) % num_nodes);
        group->disable_node((leader1 + 4) % num_nodes);

        group->restart((leader1 + 1) % num_nodes);
        group->restart((leader1 + 2) % num_nodes);
        group->enable_node((leader1 + 1) % num_nodes);
        group->enable_node((leader1 + 2) % num_nodes);

        mssleep(1000);

        group->restart((leader1 + 3) % num_nodes);
        group->enable_node((leader1 + 3) % num_nodes);

        group->append_new_command(10 + index, num_nodes - 2);
        index++;

        group->enable_node((leader1 + 0) % num_nodes);
        group->enable_node((leader1 + 4) % num_nodes);
    }
    group->append_new_command(1000, num_nodes);
    group->wait_commit(index, num_nodes, -1);

    delete group;
}

TEST_CASE(part3, persist3, "Partitioned leader and one follower crash, leader restarts")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    group->append_new_command(101, num_nodes);
    int leader = group->check_exact_one_leader();
    group->disable_node((leader + 2) % num_nodes);

    group->append_new_command(102, num_nodes - 1);

    group->disable_node((leader + 0) % num_nodes);
    group->disable_node((leader + 1) % num_nodes);
    
    group->enable_node((leader + 2) % num_nodes);

    group->restart((leader + 0) % num_nodes);
    group->enable_node((leader + 0) % num_nodes);

    group->append_new_command(103, 2);

    group->restart((leader + 1) % num_nodes);
    group->append_new_command(104, num_nodes);

    group->wait_commit(4, num_nodes, -1);

    delete group;
}

void figure_8_test(list_raft_group* group, int num_tries = 1000)
{
    int num_nodes = 5;
    group->append_new_command(2048, 1);
    int nup = num_nodes;
    for (int iters = 0; iters < num_tries; iters++) {
        int leader = -1;
        for (int i = 0; i < num_nodes; i++) {
            int term, index;
            if (group->nodes[i]->new_command(list_command(iters), term, index)) {
                leader = i;
            }
        }
        if (rand() % 1000 < 100) {
            mssleep(rand() % 500);
        } else {
            mssleep(rand() % 13);
        }
        if (leader != -1) {
            group->disable_node(leader);
            nup --;
        }
        if (nup < 3) {
            int s = rand() % num_nodes;
            if (!group->servers[s]->reachable()) {
                group->restart(s);
                nup ++;
            }
        }
    }

    for (int i = 0; i < num_nodes; i++) {
        group->restart(i);
    }

    group->append_new_command(1024, num_nodes);
}

TEST_CASE(part3, figure8, "Raft paper figure 8")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    figure_8_test(group);
	delete group;
}

TEST_CASE(part3, unreliable_agree, "Agreement under unreliable network")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    group->set_reliable(false);
    std::vector<std::thread*> workers;
    for (int iters = 1; iters < 50; iters++) {
        for (int j = 0; j < 4; j++) {
            std::thread* t = new std::thread([=](){
                group->append_new_command((100 * iters) + j, 1);
            });
            workers.push_back(t);
        }
        group->append_new_command(iters, 1);
    }
    for (auto worker : workers) {
        worker->join();
        delete worker;
    }
    group->append_new_command(4096, num_nodes);
    delete group;
}

TEST_CASE(part3, unreliable_figure_8, "Raft paper Figure 8 under unreliable network")
{
    int num_nodes = 5;
    list_raft_group *group = new list_raft_group(num_nodes);
    group->set_reliable(false);
    figure_8_test(group, 100);
    delete group;
}

TEST_CASE(part4, basic_snapshot, "Basic snapshot")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    int leader = group->check_exact_one_leader();
    int killed_node = (leader + 1) % num_nodes;
    group->disable_node(killed_node);
    for (int i = 1 ; i < 100; i++)
        group->append_new_command(100 + i, num_nodes - 1);
    leader = group->check_exact_one_leader();
    int other_node = (leader + 1) % num_nodes;
    if (other_node == killed_node) other_node = (leader + 2) % num_nodes;
    ASSERT(group->nodes[leader]->save_snapshot(), "leader cannot save snapshot");
    ASSERT(group->nodes[other_node]->save_snapshot(), "follower cannot save snapshot");
    mssleep(2000);
    group->enable_node(killed_node);
    leader = group->check_exact_one_leader();
    group->append_new_command(1024, num_nodes);
    ASSERT(group->states[killed_node]->num_append_logs < 90, "the snapshot does not work");
    delete group;   
}

TEST_CASE(part4, restore_snapshot, "Restore snapshot after failure")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    int leader = group->check_exact_one_leader();
    for (int i = 1 ; i < 100; i++)
        group->append_new_command(100 + i, num_nodes - 1);
    leader = group->check_exact_one_leader();
    ASSERT(group->nodes[leader]->save_snapshot(), "leader cannot save snapshot");
    mssleep(2000);
    group->restart(leader);
    group->append_new_command(1024, num_nodes);
    ASSERT(group->states[leader]->num_append_logs < 90, "the snapshot does not work");
    delete group;   
}

TEST_CASE(part4, override_snapshot, "Overrive snapshot")
{
    int num_nodes = 3;
    list_raft_group *group = new list_raft_group(num_nodes);
    int leader = group->check_exact_one_leader();
    for (int i = 1 ; i < 30; i++)
        group->append_new_command(100 + i, num_nodes - 1);
    leader = group->check_exact_one_leader();
    ASSERT(group->nodes[leader]->save_snapshot(), "leader cannot save snapshot");
    for (int i = 30 ; i < 60; i++)
        group->append_new_command(100 + i, num_nodes - 1);
    leader = group->check_exact_one_leader();
    ASSERT(group->nodes[leader]->save_snapshot(), "leader cannot save snapshot");
    mssleep(2000);
    group->restart(leader);
    group->append_new_command(1024, num_nodes);
    ASSERT(group->states[leader]->num_append_logs < 50, "the snapshot does not work");
    delete group;   
}

typedef raft_group<kv_state_machine, kv_command> kv_raft_group;

void push_kv_commands(kv_raft_group* group, int num, int begin)
{
    int leader = group->check_exact_one_leader();
    std::vector<std::shared_ptr<kv_command::result>> puts;
    for (int i = begin; i < begin + num; i++) {
        ASSERT(leader == group->check_exact_one_leader(), "Leader should not change");
        kv_command cmd(kv_command::CMD_PUT, "k" + std::to_string(i), "v" + std::to_string(i * 10));
        int term, index;
        puts.push_back(cmd.res);
        ASSERT(group->nodes[leader]->new_command(cmd, term, index), "Leader should not change");
    }
    for (int i = begin; i < begin + num; i++) {
        std::unique_lock<std::mutex> lock(puts[i]->mtx);
        if (!puts[i]->done) {
            ASSERT(puts[i]->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
            "Put command timeout");
        }
        ASSERT(puts[i]->succ, "Put command does not succ?");
        std::string ans_key = "k" + std::to_string(i);
        std::string ans_val = "v" + std::to_string(i * 10);
        ASSERT(puts[i]->key == ans_key, "Put command key doesn't match, " << ans_key << " != " << puts[i]->key);
        ASSERT(puts[i]->value == ans_val, "Put command value doesn't match, " << ans_val << " != " << puts[i]->value);
    }
}

void put_kv_pair(kv_raft_group* group, int k, bool succ)
{
    int leader = group->check_exact_one_leader();
    int term, index;
    std::string ans_key = "k" + std::to_string(k);
    std::string ans_val = "v" + std::to_string(k*10);
    kv_command cmd(kv_command::CMD_PUT, ans_key, ans_val);
    ASSERT(group->nodes[leader]->new_command(cmd, term, index), "Leader should not change");
    {
        std::unique_lock<std::mutex> lock(cmd.res->mtx);
        if (!cmd.res->done) {
            ASSERT(cmd.res->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
                "Put command timeout");
        }
        ASSERT(cmd.res->succ == succ, "Put command fails");
        if (succ) {
            ASSERT(cmd.res->key == ans_key, "Wrong key");
            ASSERT(cmd.res->value == ans_val, "Wrong value " << cmd.res->value << " vs " << ans_val);
        }
    }  
}

void del_kv_pair(kv_raft_group* group, int k, bool succ)
{
    int leader = group->check_exact_one_leader();
    int term, index;
    std::string ans_key = "k" + std::to_string(k);
    std::string ans_val = "v" + std::to_string(k * 10);
    kv_command cmd(kv_command::CMD_DEL, ans_key, "");
    ASSERT(group->nodes[leader]->new_command(cmd, term, index), "Leader should not change");
    {
        std::unique_lock<std::mutex> lock(cmd.res->mtx);
        if (!cmd.res->done) {
            ASSERT(cmd.res->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
                "Del command timeout");
        }
        ASSERT(cmd.res->succ == succ, "Del command fails");
        if (succ) {
            ASSERT(cmd.res->key == ans_key, "Wrong key");
            ASSERT(cmd.res->value == ans_val, "Wrong value");
        }
    }  
}

void check_kv_pair(kv_raft_group* group, int k, bool succ)
{
    int total_tries = 5;
    int tries = 0;
    while (tries < total_tries) {
        tries ++;
        int leader = group->check_exact_one_leader();
        int term, index;
        std::string ans_key = "k" + std::to_string(k);
        std::string ans_val = "v" + std::to_string(k * 10);
        kv_command cmd(kv_command::CMD_GET, ans_key, "");
        if (!group->nodes[leader]->new_command(cmd, term, index)) goto next_try;
        {
            std::unique_lock<std::mutex> lock(cmd.res->mtx);
            if (!cmd.res->done) {
                if(cmd.res->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::timeout)
                    goto next_try;
            }  
            ASSERT(cmd.res->succ == succ, "Get command fails " << cmd.res->succ << " vs " << succ);
            if (succ) {
                ASSERT(cmd.res->key == ans_key, "Wrong key");
                ASSERT(cmd.res->value == ans_val, "Wrong value");
            }
        }
        return;
        next_try:
            mssleep(200);
    }
    ASSERT(false, "Cannot read key-value pair");
}

TEST_CASE(part5, basic_kv, "Basic key-value store")
{
    int num_nodes = 3;
    int term, index;
    kv_raft_group *group = new kv_raft_group(num_nodes);
    int leader = group->check_exact_one_leader();
    push_kv_commands(group, 10, 0);
    std::vector<std::shared_ptr<kv_command::result>> reads;
    for (int i = 0; i < 10; i++) {
        ASSERT(leader == group->check_exact_one_leader(), "Leader should not change");
        kv_command cmd(kv_command::CMD_GET, "k" + std::to_string(i), "");
        reads.push_back(cmd.res);
        ASSERT(group->nodes[leader]->new_command(cmd, term, index), "Leader should not change");  
    }
    for (int i = 0; i < 10; i++) {
        std::unique_lock<std::mutex> lock(reads[i]->mtx);
        if (!reads[i]->done) {
            ASSERT(reads[i]->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
            "Get command timeout");
        }
        ASSERT(reads[i]->succ, "Get command does not succ?");
        std::string ans_key = "k" + std::to_string(i);
        std::string ans_val = "v" + std::to_string(i * 10);
        ASSERT(reads[i]->key == ans_key, "Get command key doesn't match, " << ans_key << " != " << reads[i]->key);
        ASSERT(reads[i]->value == ans_val, "Get command value doesn't match, " << ans_val << " != " << reads[i]->value);
    }
    std::string read_key = "k" + std::to_string(5);
    kv_command del_cmd(kv_command::CMD_DEL, read_key, "");
    kv_command read_cmd(kv_command::CMD_GET, read_key, "");
    std::shared_ptr<kv_command::result> del = del_cmd.res;
    std::shared_ptr<kv_command::result> rd = read_cmd.res;
    
    ASSERT(group->nodes[leader]->new_command(del_cmd, term, index), "Leader should not change");  
    ASSERT(group->nodes[leader]->new_command(read_cmd, term, index), "Leader should not change");  
    {
        std::unique_lock<std::mutex> lock(del->mtx);
        if (!del->done) {
            ASSERT(del->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
            "Del command timeout");
        }
        ASSERT(del->key == read_key, "Del command key doesn't match, " << read_key << " != " << del->key);
        ASSERT(del->succ, "Del command does not succ?");
    }
    {
        std::unique_lock<std::mutex> lock(rd->mtx);
        if (!rd->done) {
            ASSERT(rd->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) == std::cv_status::no_timeout,
            "Get command timeout");
        }
        ASSERT(rd->key == read_key, "Get command key doesn't match, " << read_key << " != " << rd->key);
        ASSERT(rd->succ == false, "Get succ on deleted key?");
    }
    delete group;
}

TEST_CASE(part5, persist_kv, "Persist key-value store")
{
    int num_nodes = 3;
    kv_raft_group *group = new kv_raft_group(num_nodes);
    for (int i = 0; i < 10; i++)
        put_kv_pair(group, i, true);
    del_kv_pair(group, 0, true);
    for (int i = 0; i < num_nodes; i++) {
        group->disable_node(i);
        group->restart(i);
    }
    mssleep(2000); // wait for election
    for (int i = 1; i < 10; i++)
        check_kv_pair(group, i, true);
    check_kv_pair(group, 0, false);
    delete group;
}

TEST_CASE(part5, snapshot_kv, "Persist key-value snapshot")
{
    int num_nodes = 3;
    kv_raft_group *group = new kv_raft_group(num_nodes);
    int leader = group->check_exact_one_leader();
    int killed_node = (leader + 1) % num_nodes;
    group->disable_node(killed_node);
    for (int i = 0; i < 50; i++)
        put_kv_pair(group, i, true);
    leader = group->check_exact_one_leader();
    int other_node = (leader + 1) % num_nodes;
    if (other_node == killed_node) other_node = (leader + 2) % num_nodes;
    ASSERT(group->nodes[leader]->save_snapshot(), "leader cannot save snapshot");
    ASSERT(group->nodes[other_node]->save_snapshot(), "follower cannot save snapshot");
    mssleep(2000);
    group->enable_node(killed_node);
    mssleep(2000); // wait for the election
    leader = group->check_exact_one_leader();
    for (int i = 0; i < 50; i++)
        check_kv_pair(group, i, true);
    delete group;
}

int main(int argc, char** argv) {
    unit_test_suite::instance()->run(argc, argv);
    return 0;
}