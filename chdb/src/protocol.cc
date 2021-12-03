#include "protocol.h"

namespace chdb_protocol {
    marshall &
    operator<<(marshall &m, const dummy_var &var) {
        m << var.v0;
        m << var.v1;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, dummy_var &var) {
        u >> var.v0;
        u >> var.v1;
        return u;
    }

    marshall &
    operator<<(marshall &m, const operation_var &var) {
        m << var.key;
        m << var.value;
        m << var.tx_id;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, operation_var &var) {
        u >> var.key;
        u >> var.value;
        u >> var.tx_id;
        return u;
    }

    marshall &
    operator<<(marshall &m, const prepare_var &var) {
        m << var.tx_id;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, prepare_var &var) {
        u >> var.tx_id;
        return u;
    }

    marshall &
    operator<<(marshall &m, const check_prepare_state_var &var) {
        m << var.tx_id;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, check_prepare_state_var &var) {
        u >> var.tx_id;
        return u;
    }

    marshall &
    operator<<(marshall &m, const commit_var &var) {
        m << var.tx_id;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, commit_var &var) {
        u >> var.tx_id;
        return u;
    }

    marshall &
    operator<<(marshall &m, const rollback_var &var) {
        m << var.tx_id;
        return m;
    }

    unmarshall &
    operator>>(unmarshall &u, rollback_var &var) {
        u >> var.tx_id;
        return u;
    }
}