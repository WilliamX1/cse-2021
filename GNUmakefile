LAB=2
SOL=0
RPC=./rpc
LAB1GE=$(shell expr $(LAB) \>\= 1)
LAB2GE=$(shell expr $(LAB) \>\= 2)
LAB3GE=$(shell expr $(LAB) \>\= 3)
LAB4GE=$(shell expr $(LAB) \>\= 4)
CXXFLAGS = -std=c++11 -g -MMD -Wall -I. -I$(RPC) -DLAB=$(LAB) -DSOL=$(SOL) -D_FILE_OFFSET_BITS=64 -no-pie
FUSEFLAGS= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25 -I/usr/local/include/fuse -I/usr/include/fuse
RPCLIB=librpc.a

ifeq ($(shell uname -s),Darwin)
  MACFLAGS= -D__FreeBSD__=10
else
  MACFLAGS=
endif
LDFLAGS = -L. -L/usr/local/lib
LDLIBS = -lpthread 
ifeq ($(LAB1GE),1)
  ifeq ($(shell uname -s),Darwin)
    ifeq ($(shell sw_vers -productVersion | sed -e "s/.*\(10\.[0-9]\).*/\1/"),10.6)
      LDLIBS += -lfuse_ino64
    else
      LDLIBS += -lfuse
    endif
  else
    LDLIBS += -lfuse
  endif
endif
LDLIBS += $(shell test -f `gcc -print-file-name=librt.so` && echo -lrt)
LDLIBS += $(shell test -f `gcc -print-file-name=libdl.so` && echo -ldl)
CC = g++
CXX = g++

lab:  lab$(LAB)
lab1: part1_tester chfs_client
lab2: chfs_client extent_server test-lab2-part1-g mr_coordinator mr_worker mr_sequential

rpclib=rpc/rpc.cc rpc/connection.cc rpc/pollmgr.cc rpc/thr_pool.cc rpc/jsl_log.cc gettime.cc
rpc/librpc.a: $(patsubst %.cc,%.o,$(rpclib))
	rm -f $@
	ar cq $@ $^
	ranlib rpc/librpc.a

rpc/rpctest=rpc/rpctest.cc
rpc/rpctest: $(patsubst %.cc,%.o,$(rpctest)) rpc/$(RPCLIB)

part1_tester=part1_tester.cc extent_client.cc extent_server.cc inode_manager.cc
part1_tester : $(patsubst %.cc,%.o,$(part1_tester))
chfs_client=chfs_client.cc extent_client.cc fuse.cc extent_server.cc inode_manager.cc

chfs_client : $(patsubst %.cc,%.o,$(chfs_client)) rpc/$(RPCLIB)

extent_server=extent_server.cc extent_smain.cc inode_manager.cc
extent_server : $(patsubst %.cc,%.o,$(extent_server)) rpc/$(RPCLIB)

test-lab2-part1-b=test-lab2-part1-b.c
test-lab2-part1-b:  $(patsubst %.c,%.o,$(test-lab2-part1-b)) rpc/$(RPCLIB)

test-lab2-part1-c=test-lab2-part1-c.c

mr_sequential=mr_sequential.cc
mr_sequential : $(patsubst %.cc,%.o,$(mr_sequential))

mr_coordinator=mr_coordinator.cc
mr_coordinator : $(patsubst %.cc,%.o,$(mr_coordinator)) rpc/$(RPCLIB)

mr_worker=mr_worker.cc
mr_worker : $(patsubst %.cc,%.o,$(mr_worker)) rpc/$(RPCLIB)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

fuse.o: fuse.cc
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(MACFLAGS) $<

# mklab.inc is needed by 6.824 staff only. Just ignore it.
-include mklab.inc

-include *.d
-include rpc/*.d

clean_files=rpc/rpctest rpc/*.o rpc/*.d *.o *.d chfs_client extent_server rpctest test-lab2-part1-a test-lab2-part1-b test-lab2-part1-c test-lab2-part1-g part1_tester demo_client demo_server mr_coordinator mr_worker mr_sequential rpc/$(RPCLIB)
.PHONY: clean handin
clean: 
	rm $(clean_files) -rf 

handin_ignore=$(clean_files) core* *log
handin_file=lab$(LAB).tgz
labdir=$(shell basename $(PWD))
handin: 
	@bash -c "cd ../; tar -X <(tr ' ' '\n' < <(echo '$(handin_ignore)')) -czvf $(handin_file) $(labdir); mv $(handin_file) $(labdir); cd $(labdir)"
	@echo Please modify lab2.tgz to lab2_[your student id].tgz and upload it to Canvas
	@echo Thanks!

rpcdemo: demo_server demo_client

demo_client:
	$(CXX) $(CXXFLAGS) demo_client.cc rpc/$(RPCLIB) $(LDFLAGS) $(LDLIBS) -o demo_client

demo_server:
	$(CXX) $(CXXFLAGS) demo_server.cc rpc/$(RPCLIB) $(LDFLAGS) $(LDLIBS) -o demo_server
