#include "exp.h"

struct timespec start, end;
struct timespec start_set[CLIENT_PER_NODE], end_set[CLIENT_PER_NODE];
double total_time;
double time_op_set[1000000];

static uint32_t count_run_ops = 0;

double printLatency() {
    double latency = (end.tv_sec - start.tv_sec) * NS_RATIO + (double)(end.tv_nsec - start.tv_nsec) / 1000;
    time_op_set[count_run_ops++] = latency;
    return latency;
}

double addToTotalLatency() {
    total_time += printLatency();
    // std::cout << "total latency (us): " << total << std::endl;
    return total_time;
}

double printTotalLatency() {
    printf("total latency (us): %f.\n", total_time);
    return total_time;
}

void resetTotalLatency() {
    total_time = 0;
}

double printAverageLatency(uint32_t num_ops) {
    double average_lat = total_time / num_ops;
    printf("average latency (us): %f.\n", average_lat);
    return average_lat;
}

double printStandardDev(uint32_t num_ops) {
    double sum_2 = 0;
    double dev_lat = 0;
    double avg_lat = printAverageLatency(num_ops);
    if (num_ops < 1000) {
        dev_lat = -1;
    }
    for (uint32_t i = 1000;i < num_ops;i++) {
        sum_2 += pow(time_op_set[i] - avg_lat, 2);
    }
    dev_lat = sum_2 / (num_ops-1000);
    dev_lat = sqrt(dev_lat);
    printf("deviation latency (us): %f.\n", dev_lat);
    return dev_lat;
}

double printThroughput(uint32_t num_ops) {
    double throughput = num_ops / total_time;
    printf("throughput (MOps/s): %f.\n", throughput);
    return throughput;
}

double printThr4Client(int order, uint32_t num_ops) {
    double latency = (end_set[order].tv_sec - start_set[order].tv_sec) * NS_RATIO + (double)(end_set[order].tv_nsec - start_set[order].tv_nsec) / 1000;
    double throughput = num_ops / latency;
    printf("client %d: throughput (MOps/s): %f.\n", order, throughput);
    return throughput;
}

void generaterandomKey(char *key) {
    int i;
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    for (i = 0;i < VALUE_SIZE-1;i++) {
        key[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    key[i] = '\0';
}

void generateKeyValueSet(KeyValue *kv, uint32_t kv_num) {
    for (uint32_t i = 0;i < kv_num;i++) {
        generaterandomKey(kv[i].key);
    }
}

void storeKeyValueSet(KeyValue *kv, uint32_t kv_num, uint32_t op_num) {
    FILE *fp = NULL;
    int randnum;

    // raw data to be inserted
    fp = fopen("./workloads/raw.txt", "w+");
    for (uint32_t i = 0;i < kv_num;i++) {
        fputs(kv[i].key, fp); fputs("\n", fp);
    }
    fclose(fp);

    // ycsb-c
    fp = fopen("./workloads/ycsb-c.txt", "w+");
    for (uint32_t i = 0;i < op_num;i++) {
        randnum = rand() % kv_num;
        fputs(kv[randnum].key, fp);fputs("\n", fp);
    }
    fclose(fp);

    // ycsb-b
    fp = fopen("./workloads/ycsb-b.txt", "w+");
    for (uint32_t i = 0;i < op_num;i++) {
        randnum = rand() % 100;
        if (randnum < 95) {
            randnum = rand() % kv_num;
            fputs("R ", fp);fputs(kv[randnum].key, fp);fputs("\n", fp);
        }
        else {
            randnum = rand() % kv_num;
            fputs("U ", fp);fputs(kv[randnum].key, fp);fputs("\n", fp);
        }
    }
    fclose(fp);

    // ycsb-a
    fp = fopen("./workloads/ycsb-a.txt", "w+");
    for (uint32_t i = 0;i < op_num;i++) {
        randnum = rand() % 100;
        if (randnum < 50) {
            randnum = rand() % kv_num;
            fputs("R ", fp);fputs(kv[randnum].key, fp);fputs("\n", fp);
        }
        else {
            randnum = rand() % kv_num;
            fputs("U ", fp);fputs(kv[randnum].key, fp);fputs("\n", fp);
        }
    }
    fclose(fp);

    // write-only (insert or update)
    fp = fopen("./workloads/ycsb-write.txt", "w+");
    for (uint32_t i = 0;i < op_num;i++) {
        randnum = rand() % kv_num;
        fputs(kv[randnum].key, fp);fputs("\n", fp);
    }
    fclose(fp);
}


void clientRunWorkload(int type, uint32_t op_num) {
    std::thread threads[CLIENT_PER_NODE];
    for (int i = 0;i < CLIENT_PER_NODE;i++) {
        // execute in parallel
        threads[i] = std::thread(runWorkload, type, op_num, i);
        threads[i].join();
    }
}

// what is strncomp
// what is getline
void runWorkload(int type, uint32_t op_num, int client_order) {
    int ret;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t len = 0;
    char *key = (char *)malloc(KEY_SIZE);
    char *value = (char *)malloc(VALUE_SIZE);

    // ycsb-c
    if (type == 0) {
        fp = fopen("./workloads/ycsb-c.txt", "r");
        if (fp == NULL)
            exit(-1);
        startTimer(start_set[client_order]);
        for (uint32_t i = 0;i < op_num;i++) {
            if (getline(&buf, &len, fp) != -1) {
                memcpy(key, buf, KEY_SIZE);
                ret = client_search(client_order, key, value);
            }
        }
        endTimer(end_set[client_order]);
        printThr4Client(client_order, op_num);
    }

    // ycsb-b
    else if (type == 1) {
        fp = fopen("./workloads/ycsb-b.txt", "r");
        if (fp == NULL)
            exit(-1);
        for (uint32_t i = 0;i < op_num;i++) {
            if (getline(&buf, &len, fp) != -1) {
                if (strncmp(buf, "R", 1) == 0) {
                    memcpy(key, buf+2, KEY_SIZE);
                    ret = client_search(client_order, buf, value);
                }
                else {
                    memcpy(key, buf+2, KEY_SIZE);
                    memcpy(value, buf+2, VALUE_SIZE);
                    ret = client_update(client_order, key, value);
                }
            }
        }
    }

    // ycsb-a
    else if (type == 2) {
        fp = fopen("./workloads/ycsb-a.txt", "r");
        if (fp == NULL)
            exit(-1);
        for (uint32_t i = 0;i < op_num;i++) {
            if (getline(&buf, &len, fp) != -1) {
                if (strncmp(buf, "R", 1) == 0) {
                    memcpy(key, buf, KEY_SIZE);
                    ret = client_search(client_order, key, value);
                }
                else {
                    memcpy(key, buf+2, KEY_SIZE);
                    memcpy(value, buf+2, VALUE_SIZE);
                    ret = client_update(client_order, key, value);
                }
            }
        }
    }

    // write-only
    else if (type == 3) {
        fp = fopen("./workloads/ycsb-write.txt", "r");
        if (fp == NULL)
            exit(-1);
        for (uint32_t i = 0;i < op_num;i++) {
            if (getline(&buf, &len, fp) != -1) {
                memcpy(key, buf, KEY_SIZE);
                memcpy(value, buf, VALUE_SIZE);
                ret = client_update(client_order, key, value);
            }
        }
    }
    else {
        fp = fopen("./workloads/raw.txt", "r");
        if (fp == NULL)
            exit(-1);
        for (uint32_t i = 0;i < op_num;i++) {
            if (getline(&buf, &len, fp) != -1) {
                memcpy(key, buf, KEY_SIZE);
                memcpy(value, buf, VALUE_SIZE);
                ret = client_insert(client_order, key, value);
            }
        }

    }
}

void localRead(uint32_t num) {
    count_run_ops = 0;

    int ret;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t len = 0;
    char *key = (char *)malloc(KEY_SIZE);
    char *value = (char *)malloc(VALUE_SIZE);


    fp = fopen("./workloads/ycsb-c.txt", "r");
    if (fp == NULL)
        exit(-1);
    for (uint32_t i = 0;i < num;i++) {
        if (getline(&buf, &len, fp) != -1) {
            memcpy(key, buf, KEY_SIZE);
            startTimer(start);
            ret = server_search(key, value);
            endTimer(end);
            addToTotalLatency();
        }
    }
    printStandardDev(num);
    resetTotalLatency();
}

void printEachOpLat(uint32_t num) {
    FILE *fp = NULL;
    fp = fopen("./workloads/help-single-line-lat.txt", "w+");
    char *buffer = (char *)malloc(20);
    for (uint32_t i = 0;i < num;i++) {
            sprintf(buffer, "%d: %f", i, time_op_set[i]);
            fputs(buffer, fp);fputs("\n", fp);
    } 
    fclose(fp);
}

void localInsert(uint32_t num) {
    count_run_ops = 0;

    int ret;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t len = 0;
    char *key = (char *)malloc(KEY_SIZE);
    uint32_t valid_count = 0;

    fp = fopen("./workloads/raw.txt", "r");
    if (fp == NULL)
        exit(-1);
    for (uint32_t i = 0;i < num;i++) {
        if (getline(&buf, &len, fp) != -1) {
            memcpy(key, buf, KEY_SIZE);
            startTimer(start);
            ret = server_insert(key, key);
            if (ret == 0) {
                valid_count++;
            }
            endTimer(end);
            addToTotalLatency();
        }
    }
    printf("valid count: %d: ", valid_count);
    printStandardDev(num);
    // printEachOpLat(num);
    resetTotalLatency();
}

void randomRead(uint32_t num) {
    int ret;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t len = 0;
    char *key = (char *)malloc(KEY_SIZE);
    char *value = (char *)malloc(VALUE_SIZE);


    fp = fopen("./workloads/raw.txt", "r");
    if (fp == NULL)
        exit(-1);
    for (uint32_t i = 0;i < num;i++) {
        if (getline(&buf, &len, fp) != -1) {
            memcpy(key, buf, KEY_SIZE);
            startTimer(start);
            ret = client_search(0, key, value);
            endTimer(end);
            addToTotalLatency();
        }
    }
    printStandardDev(num);
    resetTotalLatency();
    printEachOpLat(num);
}

void randomWrite(uint32_t num, int opcode) {
    int ret;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t len = 0;
    char *key = (char *)malloc(KEY_SIZE);
    char *value = (char *)malloc(VALUE_SIZE);


    fp = fopen("./workloads/ycsb-write.txt", "r");
    if (fp == NULL)
        exit(-1);
    for (uint32_t i = 0;i < num;i++) {
        if (getline(&buf, &len, fp) != -1) {
            memcpy(key, buf, KEY_SIZE);
            memcpy(value, buf, VALUE_SIZE);
            startTimer(start);
            ret = client_write(0, key, value, opcode);
            endTimer(end);
            // sfence();
            addToTotalLatency();
        }
    }
    printStandardDev(num);
    resetTotalLatency();
}
