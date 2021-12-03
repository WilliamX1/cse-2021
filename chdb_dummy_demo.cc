#include "chdb/src/tx_region.h"

int main() {
    chdb store(30, CHDB_PORT);

    for (int i = 0; i < 4; ++i) {
        tx_region db_client(&store);
        // dummy request
        int r = db_client.dummy();
    }
}