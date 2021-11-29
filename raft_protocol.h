#ifndef raft_protocol_h
#define raft_protocol_h

#include "rpc.h"
#include "raft_state_machine.h"

enum raft_rpc_opcodes {
    op_request_vote = 0x1212,
    op_append_entries = 0x3434,
    op_install_snapshot = 0x5656
};

enum raft_rpc_status {
   OK,
   RETRY,
   RPCERR,
   NOENT,
   IOERR
};

class request_vote_args {
public:
    // Your code here
    int term; /* candidate's term */
    int candidateId; /* candidate requesting vote */
    int lastLogIndex; /* index of candidate's last log entry */
    int lastLogTerm; /* term of candidate's last log entry */
    request_vote_args() {};
    request_vote_args(int term, int candidateId, int lastLogIndex, int lastLogTerm)
    : term(term), candidateId(candidateId), lastLogIndex(lastLogIndex), lastLogTerm(lastLogTerm) {};
};

marshall& operator<<(marshall &m, const request_vote_args& args);
unmarshall& operator>>(unmarshall &u, request_vote_args& args);


class request_vote_reply {
public:
    // Your code here
    int term; /* voter's currentTerm, for candidate to update itself */
    bool voteGranted; /* true means candidate received vote */
    request_vote_reply() {};
    request_vote_reply(int term, bool voteGranted)
    : term(term), voteGranted(voteGranted) {};
};

marshall& operator<<(marshall &m, const request_vote_reply& reply);
unmarshall& operator>>(unmarshall &u, request_vote_reply& reply);

template<typename command>
class log_entry {
public:
    // Your code here
    command cmd;
    int term;
    int index;

    log_entry() {};

    log_entry(command cmd, int term, int index)
    : cmd(cmd), term(term), index(index) {};
};

template<typename command>
marshall& operator<<(marshall &m, const log_entry<command>& entry) {
    // Your code here
    m << entry.cmd << entry.term << entry.index;
    return m;
}

template<typename command>
unmarshall& operator>>(unmarshall &u, log_entry<command>& entry) {
    // Your code here
    u >> entry.cmd >> entry.term >> entry.index;
    return u;
}

template<typename command>
class append_entries_args {
public:
    // Your code here
    bool heartbeat;
    int term; /* leader's term */
    int leaderId; /* so follower can redirect clients */
    int prevLogIndex; /* index of log entry immediately preceding new ones */
    int prevLogTerm; /* term of prevLogIndex entry */
    std::vector< log_entry<command> > entries; /* log entries to store(empty for heartbeat; may send more than one for efficiency */
    int leaderCommit; /* leader's commitIndex */

    append_entries_args() {};
    append_entries_args(bool heartbeat, int term) : heartbeat(heartbeat), term(term) {};
    append_entries_args(bool heartbeat, int term, int leaderId, int prevLogIndex, int prevLogTerm, std::vector< log_entry<command> > entries, int leaderCommit)
    : heartbeat(heartbeat), term(term), leaderId(leaderId), prevLogIndex(prevLogIndex), prevLogTerm(prevLogTerm), entries(entries), leaderCommit(leaderCommit) {};
};

template<typename command>
marshall& operator<<(marshall &m, const append_entries_args<command>& args) {
    // Your code here
    m << args.heartbeat << args.term << args.leaderId << args.prevLogIndex << args.prevLogTerm
        << args.entries << args.leaderCommit;
    return m;
}

template<typename command>
unmarshall& operator>>(unmarshall &u, append_entries_args<command>& args) {
    // Your code here
    u >> args.heartbeat >> args.term >> args.leaderId >> args.prevLogIndex >> args.prevLogTerm
        >> args.entries >> args.leaderCommit;
    return u;
}

class append_entries_reply {
public:
    // Your code here
    int term;
    bool success;
    append_entries_reply() {};
    append_entries_reply(int term, bool success) : term(term), success(success) {};
};

marshall& operator<<(marshall &m, const append_entries_reply& reply);
unmarshall& operator>>(unmarshall &m, append_entries_reply& reply);


class install_snapshot_args {
public:
    // Your code here
};

marshall& operator<<(marshall &m, const install_snapshot_args& args);
unmarshall& operator>>(unmarshall &m, install_snapshot_args& args);


class install_snapshot_reply {
public:
    // Your code here
};

marshall& operator<<(marshall &m, const install_snapshot_reply& reply);
unmarshall& operator>>(unmarshall &m, install_snapshot_reply& reply);


#endif // raft_protocol_h