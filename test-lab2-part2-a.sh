#!/bin/bash

##########################################
#  this file contains:
#   wc test
###########################################

DIR=$1

cp ./novels/*.txt $DIR

# generate the wc output
./mr_sequential $DIR/*.txt > $DIR/mr-out-0 || exit
sort $DIR/mr-out-0 > $DIR/mr-wc
rm -f $DIR/mr-out*

# compare with the correct output
if cmp $DIR/mr-wc ./novels/mr-wc-correct
then
	echo "Passed mr-wc test."
else
	exit
fi

