#!/bin/bash
make clean &> /dev/null 2>&1
echo "Building lab3 tests"
make &> /dev/null 2>&1 

score=0

raft_test_case() {
	i=1
    echo "Testing " $1"."$2;
	while(( $i<=3 ))
	do
		./raft_test $1 $2 | grep -q "Pass ("$1"."$2")";
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

raft_test_case part1 leader_election 10
raft_test_case part1 re_election 10
raft_test_case part2 basic_agree 10
raft_test_case part2 fail_agree 10
raft_test_case part2 fail_no_agree 5
raft_test_case part2 concurrent_start 5
raft_test_case part2 rejoin 5
raft_test_case part2 backup 5
raft_test_case part2 rpc_count 5
raft_test_case part3 persist1 5
raft_test_case part3 persist2 5
raft_test_case part3 persist3 5
raft_test_case part3 figure8 2
raft_test_case part3 unreliable_agree 2
raft_test_case part3 unreliable_figure_8 1
raft_test_case part4 basic_snapshot 2
raft_test_case part4 restore_snapshot 2
raft_test_case part4 override_snapshot 1
raft_test_case part5 basic_kv 4
raft_test_case part5 persist_kv 3
raft_test_case part5 snapshot_kv 3

echo "Final score:" $score "/100"