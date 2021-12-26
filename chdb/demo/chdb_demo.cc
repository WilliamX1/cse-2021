#include "../src/tx_region.h"

int main() {
    chdb store(30, CHDB_PORT);
    int r;
    {
        tx_region db_client(&store);
        db_client.put(1, 1024);
        r = db_client.get(1);
        printf("Get first\tresp:%d\n", r);
    }

    {
        tx_region db_client(&store);
        r = db_client.get(1);
        printf("Get final\tresp:%d\n", r);
    }
}