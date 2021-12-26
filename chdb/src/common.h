#ifndef common_h
#define common_h
/* Outer dependency  */
#include "rpc.h"
#include "raft.h"

#include <set>
#include <map>

/* Macro for lab4 */
#define RAFT_GROUP 1            /* Use raft group for view server. Enable it in part2 */
#define BIG_LOCK 0              /* Use big lock. Please disable it to pass the last 3 (bonus) testcases */

#include "../../raft_test_utils.h"

/* Other macros */
#define CHDB_PORT 8041


/**
 * Log entry for logging
 * */
class chdb_log {
public:
    int tx_id;
    int key;
    int new_v;
    int old_v;
};

/**
 * Abstraction of rpc server/client
 * */
class rpc_node {
public:
    rpcs *rpc_server;                       /* For receive rpc request and send back reply */

    std::map<int, rpcc *> rpc_clients;         /* For send rpc request and receive rpc reply */


    rpc_node(const int rpcs_port) :
            rpc_server(new rpcs(rpcs_port)) {
    }

    /**
     * Bind this rpc endpoint to another one by `remote_port`.
     * Return the bound port (expected to be the origin parameter)
     * */
    int bind_remote_node(const int remote_port) {
        struct sockaddr_in sin;
        make_sockaddr(std::to_string(remote_port).c_str(), &sin);
        rpcc *client = new rpcc(sin);
        assert(client->bind() >= 0);
        this->rpc_clients[remote_port] = client;
        return remote_port;
    }

    int port() const {
        return this->rpc_server->port();
    }

    /**
     * Reg one rpc request handler
     * */
    template<class S, class A1, class R>
    void reg(unsigned int proc, S *sob, int(S::*meth)(const A1 a1, R &r)) {
        this->rpc_server->template reg(proc, sob, meth);
    }

    /**
     * Post one rpc request to the node binding to `port`
     * */
    template<class R, class A1>
    int call(const int port, unsigned int proc, const A1 &a1, R &r) {
        return this->rpc_clients[port]->template call(proc, a1, r);
    }

    ~rpc_node() {
        for (auto &rpc_client: this->rpc_clients) {
            rpc_client.second->cancel();
            delete rpc_client.second;
        }
        this->rpc_server->unreg_all();
        delete this->rpc_server;
    }
};


#endif //common_h
