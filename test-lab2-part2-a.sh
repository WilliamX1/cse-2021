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
sort ./novels/mr-wc-correct > ./novels/lcoal-mr-wc-correct # in case of WSL
which tofrodos >> /dev/null || sudo apt-get install tofrodos # in case of UNIX/DOS encoding
find ./novels -type f -exec fromdos {} \;

if cmp $DIR/mr-wc ./novels/lcoal-mr-wc-correct
then
	echo "Passed mr-wc test."
else
	exit
fi
