#include "raft_protocol.h"

marshall& operator<<(marshall &m, const request_vote_args& args) {
    // Your code here

    return m;

}
unmarshall& operator>>(unmarshall &u, request_vote_args& args) {
    // Your code here

    return u;
}

marshall& operator<<(marshall &m, const request_vote_reply& reply) {
    // Your code here

    return m;
}

unmarshall& operator>>(unmarshall &u, request_vote_reply& reply) {
    // Your code here

    return u;
}

marshall& operator<<(marshall &m, const append_entries_reply& reply) {
    // Your code here

    return m;
}
unmarshall& operator>>(unmarshall &m, append_entries_reply& reply) {
    // Your code here

    return m;
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