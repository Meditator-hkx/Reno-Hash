#pragma once

#include "config.h"
#include "ib.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zconf.h>
#include <netdb.h>
#include <netinet/in.h>

typedef struct {
    uint32_t node;
    IBInfo_Client ibInfoClient;
} ClientSendInfo;

typedef struct {
    IBInfo_Server ibInfoServer;
    uint64_t Super;
    uint64_t data_index_addr;
    uint64_t data_line_addr;
    uint32_t index_num;
} ServerSendInfo;

typedef struct {
    int sock_num;
    int clientnode[NODE_NUM-1];
    int socket[NODE_NUM-1];
} SockArrary;

int buildTcpConn();
int buildClient();
int buildServer();
int sendmsg();
int recvmsg();
int sock_read(int sockfd, void *buffer, uint32_t len);
int sock_write(int sockfd, void *buffer, uint32_t len);

