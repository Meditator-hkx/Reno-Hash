#pragma once

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "storage.h"
#include "api.h"
#include <thread>

// extern struct timespec start, end;
extern struct timespec start, end;
extern struct timespec start_set[CLIENT_PER_NODE], end_set[CLIENT_PER_NODE];
extern double total_time;
extern double time_op_set[1000000];

#define startTimer(start) (clock_gettime(CLOCK_REALTIME, &start))
#define endTimer(end) (clock_gettime(CLOCK_REALTIME, &end))
#define NS_RATIO (1000UL * 1000)

double printLatency();
double addToTotalLatency();
double printTotalLatency();
void resetTotalLatency();
double printThroughput(uint32_t num_ops);
double printStandardDev(uint32_t num_ops);
void printEachOpLat(uint32_t num_ops);

double printThr4Client(int order, uint32_t num_ops);

void generaterandomKey(char *key);
void generateKeyValueSet(KeyValue *kv, uint32_t num);
void storeKeyValueSet(KeyValue *kv, int type, uint32_t kv_num, uint32_t op_num);
void runWorkload(int type, uint32_t op_num, int thread_order);
void clientRunWorkload(int type, uint32_t op_num);

void localRead(uint32_t num);
void localInsert(uint32_t num);
void randomRead(uint32_t num);
void randomWrite(uint32_t num, int opcode);


