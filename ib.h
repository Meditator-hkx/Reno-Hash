#pragma once

#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <endian.h>
#include <byteswap.h>
#include "config.h"
#include "storage.h"

#define IB_PORT (1)
#define IB_SL 0
#define IB_MTU IBV_MTU_4096

#define IB_BUF_SIZE (sizeof(ExchangeInfo))
#define GLOBAL_IB_BUF_SIZE (sizeof(GlobalExchangeInfo))
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll (uint64_t x) {return bswap_64(x); }
static inline uint64_t ntohll (uint64_t x) {return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll (uint64_t x) {return x; }
static inline uint64_t ntohll (uint64_t x) {return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif


/**
 * for a 2-node cluster, there only needs 1 QP; while for mutiple-node cluster, there needs N-1 QPs
 */
typedef struct IBRes {
    struct ibv_device **device_list;
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr *mr;    // NVM mmapped area, should be set for remote access
    struct ibv_mr *readbuf_mr;   // DRAM RDMA_READ buffer area
    struct ibv_mr *recvbuf_mr;
    struct ibv_cq *send_cq;
    struct ibv_cq *recv_cq;
    struct ibv_comp_channel *channel;
    struct ibv_srq *srq;
    struct ibv_qp *server_qpset[NODE_NUM][CLIENT_PER_NODE];
    struct ibv_qp *client_qpset[CLIENT_PER_NODE];
    struct ibv_wc *wc;
    char *buf;
    char *recv_buf;
    char *read_buf;

    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;
    uint32_t rkey;
    uint64_t raddr;
} IBRes;


/**
 * node: server node number
 * qp_num: the corresponding qp number to communicate with other nodes in the server cluster \
 *  for instance: when SERVER_NUM = 3, qp_num[3] = {0, 224, 225} for Node 0
 * lid: the access id for other remote nodes to use
 * rkey: the access key for other remote nodes to use
 */
typedef struct {
    uint8_t node;
    uint32_t qp_num[NODE_NUM];
    uint16_t lid;
    uint32_t rkey;
} ExchangeInfo;


extern IBRes ib_res;

int setup_ib();
int change_qp_state_rts();
int rdma_recv(int client_order);
int rdma_srq_recv();
int rdma_send();
int rdma_read(int client_order, char *r_addr, uint32_t size, char *l_addr);
int rdma_write(int client_order, char *r_addr, uint32_t size, char *l_addr, int flag, uint32_t imm);
int post_cq(int flag, uint32_t &imm);
void close_ib();
