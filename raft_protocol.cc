#include "raft_protocol.h"

marshall& operator<<(marshall &m, const request_vote_args& args) {
    // Your code here
    m << args.term << args.candidateId << args.lastLogIndex << args.lastLogTerm;
    return m;
}
unmarshall& operator>>(unmarshall &u, request_vote_args& args) {
    // Your code here
    u >> args.term >> args.candidateId >> args.lastLogIndex >> args.lastLogTerm;
    return u;
}

marshall& operator<<(marshall &m, const request_vote_reply& reply) {
    // Your code here
    m << reply.term << reply.voteGranted;
    return m;
}

unmarshall& operator>>(unmarshall &u, request_vote_reply& reply) {
    // Your code here
    u >> reply.term >> reply.voteGranted;
    return u;
}

marshall& operator<<(marshall &m, const append_entries_reply& reply) {
    // Your code here
    m << reply.term << reply.success;
    return m;
}
unmarshall& operator>>(unmarshall &u, append_entries_reply& reply) {
    // Your code here
    u >> reply.term >> reply.success;
    return u;
}

marshall& operator<<(marshall &m, const install_snapshot_args& args) {
    // Your code here

    return m;
}

unmarshall& operator>>(unmarshall &u, install_snapshot_args& args) {
    // Your code here

    return u; 
}

marshall& operator<<(marshall &m, const install_snapshot_reply& reply) {
    // Your code here

    return m;
}

unmarshall& operator>>(unmarshall &u, install_snapshot_reply& reply) {
    // Your code here

    return u;
}