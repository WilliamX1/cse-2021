#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <mutex>

#include "mr_protocol.h"
#include "rpc.h"

using namespace std;

struct Task {
	int taskType;     // should be either Mapper or Reducer
	bool isAssigned;  // has been assigned to a worker
	bool isCompleted; // has been finised by a worker
	int index;        // index to the file
};

class Coordinator {
public:
	Coordinator(const vector<string> &files, int nReduce);
	mr_protocol::status askTask(int d, mr_protocol::AskTaskResponse& reply);
	mr_protocol::status submitTask(int taskType, int index, bool &success);
	bool isFinishedMap();
	bool isFinishedReduce();
	bool Done();

private:
	vector<string> files;
	vector<Task> mapTasks;
	vector<Task> reduceTasks;

	mutex mtx;

	long completedMapCount;
	long completedReduceCount;
	bool isFinished;
	bool isSummary;

	static long inc; /* 用作文件后缀，只增不减 */
	
	string getFile(int index);
};


// Your code here -- RPC handlers for the worker to call.

mr_protocol::status Coordinator::askTask(int d, mr_protocol::AskTaskResponse& reply) {
	// Lab2 : Your code goes here.
	/* 文件命名方式为 mr-X-Y，X 代表 mapTasks 的 index，Y 代表散列后应该被分到 reduceTask 的 index */
	
	reply.readfiles.clear();

	/* 优先分配 mapTask */
	for (unsigned int i = 0; i < mapTasks.size(); i++)
	{
		if (mapTasks[i].isCompleted || mapTasks[i].isAssigned) continue;
		else {
			int index = mapTasks[i].index;
			string readfile = getFile(index);
			
			reply.taskType = MAP;
			reply.index = index;
			reply.readfiles.push_back(readfile);

			this->mtx.lock();
			mapTasks[i].isAssigned = true;
			this->mtx.unlock();
			return mr_protocol::OK;
		};
	};

	if (!isFinishedMap()) {
		reply.taskType = NONE;
		return mr_protocol::OK;
	};

	/* 分配 reduceTask */
	for (unsigned int i = 0; i < reduceTasks.size(); i++)
	{
		if (reduceTasks[i].isCompleted || reduceTasks[i].isAssigned) continue;
		else {
			int index = reduceTasks[i].index;

			reply.taskType = REDUCE;
			reply.index = index;
			
			for (unsigned int j = 0; j < mapTasks.size(); j++)
			{
				string readfile = "mr-" + to_string(mapTasks[j].index) + '-' + to_string(reduceTasks[i].index);
				reply.readfiles.push_back(readfile);
			};
			this->mtx.lock();
			reduceTasks[i].isAssigned = true;
			this->mtx.unlock();
			return mr_protocol::OK;
		}
	}

	/* 最后汇总 */
	this->mtx.lock();
	bool isOK = this->isSummary;
	this->mtx.unlock();
	if (isFinishedMap() && isFinishedReduce() && !isOK) {
		reply.taskType = SUMMARY;
		reply.index = -1;

		fprintf(stderr, "\nSUMMAYR\n");
		for (unsigned int i = 0; i < reduceTasks.size(); i++)
		{
			string readfile = "mr-" + to_string(reduceTasks[i].index);
			reply.readfiles.push_back(readfile);
		};

		this->mtx.lock();
		this->isSummary = true;
		this->mtx.unlock();

		return mr_protocol::OK;
	};

	// /* 所有 task 都已经完成 */
	reply.taskType = NONE;

	return mr_protocol::OK;
}

mr_protocol::status Coordinator::submitTask(int taskType, int index, bool &success) {
	// Lab2 : Your code goes here.
	success = false;

	if (taskType == NONE) {
		success = true;
		return mr_protocol::OK;
	}

	else if (taskType == MAP) {
		for (unsigned int i = 0; i < mapTasks.size(); i++)
			if (mapTasks[i].index == index) {
				if (mapTasks[i].isCompleted == false) {
					this->mtx.lock();
			  		mapTasks[i].isCompleted = true;
					this->completedMapCount++;
					this->mtx.unlock();

					fprintf(stderr, "MAP, index: %d finish!\n", index);
				};
				break;
			};
		success = true;
		return mr_protocol::OK;
	}

	else if (taskType == REDUCE) {
		for (unsigned int i = 0; i < reduceTasks.size(); i++)
			if (reduceTasks[i].index == index) {
				if (reduceTasks[i].isCompleted == false) {
					this->mtx.lock();
					reduceTasks[i].isCompleted = true;
					this->completedReduceCount++;
					this->mtx.unlock();

					fprintf(stderr, "REDUCE, index: %d finish!\n", index);
				};
				break;
			};
		success = true;
		return mr_protocol::OK;
	}

	else if (taskType == SUMMARY) {
		this->mtx.lock();
		this->isFinished = true;
		this->mtx.unlock();

		fprintf(stderr, "SUMMARY, index: %d finish!\n", index);

		success = true;
		return mr_protocol::OK;
	}

	success = true;
	return mr_protocol::OK;
}

string Coordinator::getFile(int index) {
	this->mtx.lock();
	string file = this->files[index];
	this->mtx.unlock();
	return file;
}

bool Coordinator::isFinishedMap() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedMapCount >= long(this->mapTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

bool Coordinator::isFinishedReduce() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedReduceCount >= long(this->reduceTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

//
// mr_coordinator calls Done() periodically to find out
// if the entire job has finished.
//
bool Coordinator::Done() {
	bool r = false;
	this->mtx.lock();
	r = this->isFinished;
	this->mtx.unlock();
	return r;
}

//
// create a Coordinator.
// nReduce is the number of reduce tasks to use.
//
Coordinator::Coordinator(const vector<string> &files, int nReduce)
{
	this->files = files;
	this->isFinished = false;
	this->completedMapCount = 0;
	this->completedReduceCount = 0;
	this->isSummary = false;

	int filesize = files.size();
	for (int i = 0; i < filesize; i++) {
		this->mapTasks.push_back(Task{mr_tasktype::MAP, false, false, i});
	}
	for (int i = 0; i < nReduce; i++) {
		this->reduceTasks.push_back(Task{mr_tasktype::REDUCE, false, false, i});
	}
}

int main(int argc, char *argv[])
{
	int count = 0;

	if(argc < 3){
		fprintf(stderr, "Usage: %s <port-listen> <inputfiles>...\n", argv[0]);
		exit(1);
	}
	char *port_listen = argv[1];
	
	setvbuf(stdout, NULL, _IONBF, 0);

	char *count_env = getenv("RPC_COUNT");
	if(count_env != NULL){
		count = atoi(count_env);
	}

	vector<string> files;
	char **p = &argv[2];
	while (*p) {
		files.push_back(string(*p));
		++p;
	}

	rpcs server(atoi(port_listen), count);

	Coordinator c(files, REDUCER_COUNT);
	
	//
	// Lab2: Your code here.
	// Hints: Register "askTask" and "submitTask" as RPC handlers here
	// 
	server.reg(mr_protocol::asktask, &c, &Coordinator::askTask);
	server.reg(mr_protocol::submittask, &c, &Coordinator::submitTask);
	while(!c.Done()) {
		sleep(1);
	}

	return 0;
}


