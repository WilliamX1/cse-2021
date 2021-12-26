#!/usr/bin/env bash

CHFSDIR1=$PWD/chfs1
CHFSDIR2=$PWD/chfs2

export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -u";
fi
$UMOUNT $CHFSDIR1
$UMOUNT $CHFSDIR2
killall extent_server
killall chfs_client
