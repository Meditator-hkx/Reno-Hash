#pragma once

#include <stdint.h>

/* what is local node corresponding to its IP  */
#define LOCAL_NODE 0

/* which is the server node   */
#define SERVER_NODE 0

/* how many nodes */
#define NODE_NUM 2

/* how many clients for each node */
#define CLIENT_PER_NODE 8

#define PM_PATH "/home/kaixin/Codes/pmdir/pmfile"

typedef struct {
    uint32_t QPSet[NODE_NUM][CLIENT_PER_NODE];
    uint32_t LidSet;
    uint32_t RKeySet;
} IBInfo_Server;

typedef struct {
    uint32_t QPSet[NODE_NUM][CLIENT_PER_NODE];
    uint32_t LidSet[NODE_NUM];
    uint32_t RKeySet[NODE_NUM];
} IBInfo_Client;

extern const char *ServerIPSet[NODE_NUM];
extern const uint8_t ServerNodeSet[NODE_NUM];
extern const uint32_t ServerPortSet[NODE_NUM];
extern IBInfo_Server ibInfoServer;
extern IBInfo_Client ibInfoClient;
