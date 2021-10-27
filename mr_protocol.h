#ifndef mr_protocol_h_
#define mr_protocol_h_

#include <string>
#include <vector>

#include "rpc.h"

using namespace std;

#define REDUCER_COUNT 4

enum mr_tasktype {
	NONE = 0, // this flag means no task needs to be performed at this point
	MAP,
	REDUCE
};

class mr_protocol {
public:
	typedef int status;
	enum xxstatus { OK, RPCERR, NOENT, IOERR };
	enum rpc_numbers {
		asktask = 0xa001,
		submittask,
	};

	struct AskTaskResponse {
		// Lab2: Your definition here.
		mr_tasktype taskType;
		int index;
		/* 如果是 mapTask 则仅需要使用一个元素 
		 * 如果是 reduceTask 则需要多个文件
		 */
		vector<string> readfiles;
	};

	struct AskTaskRequest {
		// Lab2: Your definition here.

	};

	struct SubmitTaskResponse {
		// Lab2: Your definition here.
	};

	struct SubmitTaskRequest {
		// Lab2: Your definition here.
	};
};

inline marshall &
operator<<(marshall &m, const mr_protocol::AskTaskResponse a) {
	m << (int) a.taskType;
	m << (int) a.index;
	m << (unsigned int) a.readfiles.size();
	for (unsigned int i = 0; i < a.readfiles.size(); i++)
		m << a.readfiles[i];
	return m;
}

inline unmarshall &
operator>>(unmarshall &u, mr_protocol::AskTaskResponse &a) {

	a.taskType = NONE;
	a.index = 0;
	a.readfiles.clear();

	int taskType;
	u >> taskType;
	a.taskType = mr_tasktype(taskType);

	int index;
	u >> index;
	a.index = index;

	unsigned int n;
	u >> n;

	for (unsigned int i = 0; i < n; i++) {
		std::string str;
		u >> str;
		a.readfiles.push_back(str);
	}

	return u;
}

#endif

