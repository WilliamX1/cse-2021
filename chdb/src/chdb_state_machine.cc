#include "chdb_state_machine.h"

chdb_command::chdb_command() {
    // TODO: Your code here
}

chdb_command::chdb_command(command_type tp, const int &key, const int &value, const int &tx_id)
        : cmd_tp(tp), key(key), value(value), tx_id(tx_id) {
    // TODO: Your code here
}

chdb_command::chdb_command(const chdb_command &cmd) :
        cmd_tp(cmd.cmd_tp), key(cmd.key), value(cmd.value), tx_id(cmd.tx_id), res(cmd.res) {
    // TODO: Your code here
}


void chdb_command::serialize(char *buf, int size) const {
    // TODO: Your code here
}

void chdb_command::deserialize(const char *buf, int size) {
    // TODO: Your code here
}

marshall &operator<<(marshall &m, const chdb_command &cmd) {
    // TODO: Your code here
    return m;
}

unmarshall &operator>>(unmarshall &u, chdb_command &cmd) {
    // TODO: Your code here
    return u;
}

void chdb_state_machine::apply_log(raft_command &cmd) {
    // TODO: Your code here
}