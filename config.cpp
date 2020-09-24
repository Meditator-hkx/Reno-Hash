#include "config.h"

const char *ServerIPSet[NODE_NUM] = {
        "192.168.99.12",
        "192.168.99.12",
};

const uint32_t ServerPortSet[NODE_NUM] = {
        2345,
        2346,
};

const uint8_t ServerNodeSet[NODE_NUM] = {
        0,
        1,
};

IBInfo_Server ibInfoServer;
IBInfo_Client ibInfoClient;
