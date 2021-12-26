#!/bin/bash
make clean &> /dev/null 2>&1
echo "Building lab4 tests"
make &> /dev/null 2>&1 

score=0

run_test_case() {
	i=1
    echo "Testing " $1"."$2;
	while(( $i<=3 ))
	do
		./chdb_test $1 $2 | grep -q "Pass ("$1"."$2")";
		if [ $? -ne 0 ];
		then
            echo "Fail " $1"."$2;
			return
		fi;
		let "i++"
	done
    echo "Pass " $1"."$2;
	score=$((score+$3))
}

run_test_case part1 simple_tx_1 5
run_test_case part1 simple_tx_2 5
run_test_case part1 simple_tx_abort 5
run_test_case part1 random_tx_abort 5
run_test_case part1 shard_dispatch_test_target_one 5
run_test_case part1 shard_dispatch_test_in_static_rage 5
run_test_case part2 shard_client_replica 15
run_test_case part2 shard_client_replica_with_put 15
run_test_case part2 raft_view_server 10
run_test_case part3 tx_lock_test_1 6
run_test_case part3 tx_lock_test_2 6
run_test_case part3 tx_lock_test_3 6
run_test_case part3 tx_lock_test_4 4
run_test_case part3 tx_lock_test_5 4
run_test_case part3 tx_lock_test_6 4

echo "Final score:" $score "/100"