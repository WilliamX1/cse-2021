#!/usr/bin/env bash

ulimit -c unlimited

BASE_PORT=$RANDOM
BASE_PORT=$[BASE_PORT+2000]
EXTENT_PORT=$BASE_PORT

CHFSDIR1=$PWD/chfs1
CHFSDIR2=$PWD/chfs2

echo "starting ./extent_server $EXTENT_PORT > extent_server.log 2>&1 &"
./extent_server $EXTENT_PORT > extent_server.log 2>&1 &
sleep 1

rm -rf $CHFSDIR1
mkdir $CHFSDIR1 || exit 1
sleep 1
echo "starting ./chfs_client $CHFSDIR1 $EXTENT_PORT > chfs_client1.log 2>&1 &"
./chfs_client $CHFSDIR1 $EXTENT_PORT > chfs_client1.log 2>&1 &
sleep 1

rm -rf $CHFSDIR2
mkdir $CHFSDIR2 || exit 1
sleep 1
echo "starting ./chfs_client $CHFSDIR2 $EXTENT_PORT > chfs_client2.log 2>&1 &"
./chfs_client $CHFSDIR2 $EXTENT_PORT > chfs_client2.log 2>&1 &

sleep 2

# make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount CHFS properly at ./chfs1"
    exit -1
fi

# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/chfs2" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount CHFS properly at ./chfs2"
    exit -1
fi
