#include "hash.h"
#include "config.h"
#include "ib.h"
#include "storage.h"
#include "api.h"
#include "trxn.h"
#include "exp.h"
#include "tcp.h"

int main() {
    int ret;
    // test hashtable size: 100MB
    uint32_t test_bucket_num = 100 * 10000;

    if (LOCAL_NODE == SERVER_NODE) {
        server_init(test_bucket_num);
    }

    char *key1 = (char *)"kaixin\0";
    char *value1 = (char *)"25";
    char *value2 = (char *)"30";
    char *value_get = (char *)malloc(20);
    char *key2 = (char *)"zexuan\0";
    char *value3 = (char *)"girlfriend";
    char *key3 = (char *)"family\0";
    char *value4 = (char *)"future";

    if (LOCAL_NODE == SERVER_NODE) {
        /*
        startTimer(start);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        ret = server_insert(key1, value1);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();
        resetTotalLatency();

        startTimer(start);
        ret = server_search(key1, value_get);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();
        resetTotalLatency();

        // KeyValue *kv = (KeyValue *)malloc(10000000 *sizeof(KeyValue));
        // generateKeyValueSet(kv, 1000000);
        // storeKeyValueSet(kv, 1000000, 10000000);
        localInsert(1000000);
        localRead(1000000);

        return 0;
        */

        buildServer();
        server_check();
    }
    else {
        buildClient();

        sleep(1);

        /*
        startTimer(start);
        ret = client_search(0, key1, value_get);
        ret = client_search(0, key2, value_get);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();

        startTimer(start);
        ret = client_insert(0, key2, value3);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();

        startTimer(start);
        ret = client_update(0, key1, value2);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();

        startTimer(start);
        ret = client_update(0, key2, value4);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();

        startTimer(start);
        ret = client_del(0, key2);
        endTimer(end);
        addToTotalLatency();
        printTotalLatency();
        */

        runWorkload(0, 10000, 0);
        // clientRunWorkload(0, 1000000);
        return 0;
    }
}
