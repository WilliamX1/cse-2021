#!/usr/bin/env bash

ChFSDIR1=$PWD/chfs1

export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -u";
fi
$UMOUNT $ChFSDIR1
killall extent_server
killall chfs_client
killall lock_server
