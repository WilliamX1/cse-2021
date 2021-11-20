#!/bin/bash

##########################################
#  this file contains:
#   mr-wc test
###########################################

DIR=$1

rm $DIR/* >> /dev/null 2>&1

cd $DIR

MRCOORDINATOR=../mr_coordinator
MRWORKER=../mr_worker

timeout -k 2s 10s $MRCOORDINATOR 9876 ../novels/*.txt &
pid=$!

# give the coordinator time to create the sockets.
sleep 1

# start multiple workers.
timeout -k 1s 4s $MRWORKER 9876 ./ &
timeout -k 1s 4s $MRWORKER 9876 ./ &
timeout -k 1s 4s $MRWORKER 9876 ./ &
timeout -k 1s 4s $MRWORKER 9876 ./ &

# wait for the coordinator to exit.
wait $pid

# since workers are required to exit when a job is completely finished,
# and not before, that means the job has finished.
sort mr-out* | grep . > mr-wc-all
sort ../novels/mr-wc-correct > ../novels/lcoal-mr-wc-correct # in case of WSL
which tofrodos >> /dev/null || sudo apt-get install tofrodos # in case of UNIX/DOS encoding
find ../novels -type f -exec fromdos {} \;

if cmp mr-wc-all ../novels/lcoal-mr-wc-correct
then
	echo "Passed mr-wc-distributed test."
else
	exit
fi

# wait for remaining workers and coordinator to exit.
wait
