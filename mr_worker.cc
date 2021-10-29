#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>

#include "rpc.h"
#include "mr_protocol.h"
#include "chfs_client.h"

using namespace std;

struct KeyVal {
    string key;
    string val;

	KeyVal(){};
	KeyVal(string k, string v) : key(k), val(v) {};
};

bool isLowerLetter(char ch) { /* 判断是否是小写英文字母 */
    return 'a' <= ch && ch <= 'z';
};
bool isUpperLetter(char ch) { /* 判断是否是大写英文字母 */
    return 'A' <= ch && ch <= 'Z';
};
bool isEngAlpha(char ch) { /* 判断是否是英文字母 */
    return isLowerLetter(ch) || isUpperLetter(ch);
};

bool judgeStr(string& str) { /* 判断是否合法 */
    if (str == "") return false;
    int size = str.size();
    for (int i = 0; i < size; i++)
        if (!isEngAlpha(str[i])) return false;
    return true;
};
string strPlus(string str1, string str2) {
    return to_string(stol(str1) + stol(str2));
};
int string2reduce(string key) {
	std::hash<std::string> h;
	return h(key) % REDUCER_COUNT;
};

//
// The map function is called once for each file of input. The first
// argument is the name of the input file, and the second is the
// file's complete contents. You should ignore the input file name,
// and look only at the contents argument. The return value is a slice
// of key/value pairs.
//
vector<KeyVal> Map(const string &filename, const string &content)
{
	// Copy your code from mr_sequential.cc here.
	// Your code goes here
    // Hints: split contents into an array of words.
    vector<KeyVal> ret;
    int l = 0, r = 0; /* 下标 */
    while (content[r] != '\0') {
        if (isEngAlpha(content[r])) r++;
        else {
            string str = r > l ? content.substr(l, r - l) : "";
            if (judgeStr(str)) {
                KeyVal kv;
                kv.key = str; kv.val = "1";
                ret.push_back(kv);
            };
            while (content[r] != '\0' && !isEngAlpha(content[r])) r++;
            l = r;
        };
    };
    /* 最后一个字符串 */
    string str = r > l ? content.substr(l, r - l) : "";
    if (judgeStr(str)) {
        KeyVal kv;
        kv.key = str; kv.val = "1";
        ret.push_back(kv);
    };
    return ret;
}

//
// The reduce function is called once for each key generated by the
// map tasks, with a list of all the values created for that key by
// any map task.
//
string Reduce(const string &key, const vector < string > &values)
{
    // Copy your code from mr_sequential.cc here.
	// Your code goes here
    // Hints: return the number of occurrences of the word.
    string ret = "0";
    for (unsigned int i = 0; i < values.size(); i++)
		ret = strPlus(ret, values[i]);
    return ret;
}


typedef vector<KeyVal> (*MAPF)(const string &key, const string &value);
typedef string (*REDUCEF)(const string &key, const vector<string> &values);

class Worker {
public:
	Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf);

	void doWork();

private:
	void doMap(int index, const vector<string> &readfiles);
	void doReduce(int index, const vector<string> &readfiles);
	void doSummary(int index, const vector<string> &readfiles);
	void doSubmit(mr_tasktype taskType, int index);

	mutex mtx;
	int id;

	rpcc *cl;
	std::string basedir;
	MAPF mapf;
	REDUCEF reducef;
};


Worker::Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf)
{
	this->basedir = dir;
	this->mapf = mf;
	this->reducef = rf;

	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	this->cl = new rpcc(dstsock);
	if (this->cl->bind() < 0) {
		printf("mr worker: call bind error\n");
	}
}

void Worker::doMap(int index, const vector<string>& readfiles)
{
	fprintf(stderr, "worker: get MAP task %d\n", index);
	// Lab2: Your code goes here.
	vector <KeyVal> intermediate;

	for (unsigned int i = 0; i < readfiles.size(); i++) {
		string filename = readfiles[i];
        string content;
        // Read the whole file into the buffer.
        // printf("Read the whole file into the buffer.\n");
        // printf("%s\n", content.c_str());
        getline(ifstream(filename), content, '\0');
        // printf("Finish Read the whole file into the buffer.\n");
        vector <KeyVal> KVA = Map(filename, content);
		
		intermediate.insert(intermediate.end(), KVA.begin(), KVA.end());

	};

	FILE* write[REDUCER_COUNT];

	for (unsigned int i = 0; i < REDUCER_COUNT; i++)
	{
		string filename = this->basedir + "mr-" + to_string(index) + '-' + to_string(i);
		write[i] = fopen(filename.c_str(), "w");
		if (write[i] == NULL) {
			fprintf(stderr, "create map inter-medium file failed\n");
			exit(-1);
		};
	};

	for (unsigned int i = 0; i < intermediate.size(); i++)
	{
		int reduce = string2reduce(intermediate[i].key);
		fprintf(write[reduce], "%s %s\n", intermediate[i].key.data(), intermediate[i].val.data());
	};

	for (unsigned int i = 0; i < REDUCER_COUNT; i++)
		fclose(write[i]);
	
	return;
}

void Worker::doReduce(int index, const vector<string>& readfiles)
{
	fprintf(stderr, "worker: get REDUCE task %d\n", index);
	// Lab2: Your code goes here.
	// vector <KeyVal> intermediate;
	unordered_map<string, uint64_t> intermediate;

	for (unsigned int i = 0; i < readfiles.size(); i++) {
		string filename = this->basedir + readfiles[i];

		// string content;
        // Read the whole file into the buffer.
        // printf("Read the whole file into the buffer.\n");
        // printf("%s\n", content.c_str());
        // getline(ifstream(filename), content, '\0');
        // printf("Finish Read the whole file into the buffer.\n");

		ifstream file(filename, ios::in);
		if (!file.is_open()) {
			fprintf(stderr, "worker: file %s not open\n", filename.data());
			continue;
		};

		string key, val;
		while (file >> key >> val) {
			intermediate[key] += atoll(val.c_str());
		};
		// vector <KeyVal> KVA;
		// int start = 0, mid = 0, end = 0;
		// while (content[end] != '\0') {

		// 	mid = start;
		// 	while (content[mid] != ' ') mid++;
		// 	end = start;
		// 	while (content[end] != '\n') end++;
			
		// 	// mid = content.find_first_of(' ', start);
		// 	// end = content.find_first_of('\n', start);

		// 	string key = content.substr(start, mid - start);
		// 	int val = stol(content.substr(mid + 1, end - (mid + 1)));
			
		// 	unordered_map<string, int>::iterator iter = intermediate.find(key);
		// 	if (iter != intermediate.end()) iter->second += val;
		// 	else iter->second = val;
		// 	// KeyVal kv(content.substr(start, mid - start), content.substr(mid + 1, end - (mid + 1)));
		// 	// KVA.push_back(kv);

		// 	start = ++end;
		// };
		// intermediate.insert(intermediate.end(), KVA.begin(), KVA.end());
		file.close();
	};

    // sort(intermediate.begin(), intermediate.end(),
    // 	[](KeyVal const & a, KeyVal const & b) {
    //     // int ret = strcasecmp(a.key.c_str(), b.key.c_str());
    //     // return ret == 0 ? a.key > b.key : ret < 0;
    //     return a.key <= b.key;
	// });

	FILE* write;
	string filename = this->basedir + "mr-" + to_string(index);
	// string filename = this->basedir + "mr-out";

	write = fopen(filename.c_str(), "w");
	if (write == NULL) {
		fprintf(stderr, "create reduce inter-medium file failed\n");
		exit(-1);
	};
	
	unordered_map<string, uint64_t>::iterator iter = intermediate.begin();
	while (iter != intermediate.end()) {
		// string key = iter->first;
		// string val = to_string(iter->second);
		fprintf(write, "%s %s\n", (iter->first).data(), to_string(iter->second).data());
		iter++;
	};
	// for (unsigned int i = 0; i < intermediate.size(); i++) {
	// 	unsigned int j = i + 1;
	// 	for (; j < intermediate.size() && intermediate[j].key == intermediate[i].key;)
	// 		j++;
	// 	vector < string > values;

	// 	for (unsigned int k = i; k < j; k++) {
	// 		values.push_back(intermediate[k].val);
	// 	}

	// 	string output = Reduce(intermediate[i].key, values);
	// 	fprintf(write, "%s %s\n", intermediate[i].key.data(), output.data());

	// 	i = j;
	// };

	fclose(write);

	return;
}

void Worker::doSummary (int index, const vector<string> &readfiles) 
{
	fprintf(stderr, "worker: get SUMMARY task %d\n", index);

	FILE* write;
	string writefile = this->basedir + "mr-out";
	write = fopen(writefile.c_str(), "w");
	if (write == NULL) {
		fprintf(stderr, "create final summary file failed\n");
		exit(-1);
	};

	for (unsigned int i = 0; i < readfiles.size(); i++)
	{
		string filename = this->basedir + readfiles[i];
		string content;
		getline(ifstream(filename), content, '\0');

		fprintf(write, "%s", content.data());
	};

	fclose(write);

	return;
}

void Worker::doSubmit(mr_tasktype taskType, int index)
{
	bool b;
	mr_protocol::status ret = this->cl->call(mr_protocol::submittask, taskType, index, b);
	if (ret != mr_protocol::OK) {
		fprintf(stderr, "submit task failed\n");
		exit(-1);
	}
}

void Worker::doWork()
{
	for (;;) {

		//
		// Lab2: Your code goes here.
		// Hints: send asktask RPC call to coordinator
		// if mr_tasktype::MAP, then doMap and doSubmit
		// if mr_tasktype::REDUCE, then doReduce and doSubmit
		// if mr_tasktype::NONE, meaning currently no work is needed, then sleep
		//
		mr_protocol::AskTaskResponse reply;
		int d;
		mr_protocol::status ret = this->cl->call(mr_protocol::asktask, d, reply);
		if (ret == mr_protocol::OK) {
			if (reply.taskType == MAP) {
				doMap(reply.index, reply.readfiles);
				doSubmit(reply.taskType, reply.index);
			} else if (reply.taskType == REDUCE) {
				doReduce(reply.index, reply.readfiles);
				doSubmit(reply.taskType, reply.index);
			} else if (reply.taskType == SUMMARY) {
				doSummary(reply.index, reply.readfiles);
				doSubmit(reply.taskType, reply.index);
			} else if (reply.taskType == NONE) {
				sleep(1);
			};
		};	
	};
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <coordinator_listen_port> <intermediate_file_dir> \n", argv[0]);
		exit(1);
	}

	MAPF mf = Map;
	REDUCEF rf = Reduce;
	Worker w(argv[1], argv[2], mf, rf);
	w.doWork();

	return 0;
}

