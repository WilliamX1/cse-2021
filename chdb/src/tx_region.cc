#include "tx_region.h"


int tx_region::put(const int key, const int val) {
    // TODO: Your code here
    return 0;
}

int tx_region::get(const int key) {
    // TODO: Your code here
    return 0;
}

int tx_region::tx_can_commit() {
    // TODO: Your code here
    return chdb_protocol::prepare_ok;
}

int tx_region::tx_begin() {
    // TODO: Your code here
    printf("tx[%d] begin\n", tx_id);
    return 0;
}

int tx_region::tx_commit() {
    // TODO: Your code here
    printf("tx[%d] commit\n", tx_id);
    return 0;
}

int tx_region::tx_abort() {
    // TODO: Your code here
    printf("tx[%d] abort\n", tx_id);
    return 0;
}
