#include "ib.h"
#include "config.h"
#include "storage.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

struct ibv_device **device_list;
IBRes ib_res;

int setup_ib() {
    printf("Set up IB attributes: \n");
    int num_dev;
    int ret;

    printf("GET IB DEVICE LIST...\n");
    device_list = ibv_get_device_list(&num_dev);
    assert(device_list);

    printf("OPEN IB DEVICE...\n");
    ib_res.ctx = ibv_open_device(*device_list);
    assert(ib_res.ctx);

    printf("GET DEVICE INFORMATION...\n");
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    assert(ret == 0);

    printf("GET PORT INFORMATION...\n");
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    assert(ret == 0);

    printf("ALLOCATE PROTECTION DOMAIN...\n");
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    assert(ib_res.pd);

    printf("REGISTER MEMORY...\n");
    if (LOCAL_NODE == SERVER_NODE) {
        // debug: error occurs.
        // try 1: use smaller size for registering
        // try 2: allocate a new space region
        ib_res.mr = ibv_reg_mr(ib_res.pd, ib_res.buf, Super->table_size,
                               IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_WRITE |
                               IBV_ACCESS_REMOTE_ATOMIC);
        assert(ib_res.mr);
    }
    else {
        ib_res.read_buf = (char *)malloc(GROUP_SIZE * CLIENT_PER_NODE);
        ib_res.recv_buf = (char *)malloc(BUCKET_SIZE * CLIENT_PER_NODE);
        ib_res.readbuf_mr = ibv_reg_mr(ib_res.pd, ib_res.read_buf, GROUP_SIZE * CLIENT_PER_NODE,
                                       IBV_ACCESS_LOCAL_WRITE |
                                       IBV_ACCESS_REMOTE_READ |
                                       IBV_ACCESS_REMOTE_WRITE);
        ib_res.recvbuf_mr = ibv_reg_mr(ib_res.pd, ib_res.recv_buf, BUCKET_SIZE * CLIENT_PER_NODE,
                                       IBV_ACCESS_LOCAL_WRITE |
                                       IBV_ACCESS_REMOTE_READ |
                                       IBV_ACCESS_REMOTE_WRITE |
                                       IBV_ACCESS_REMOTE_ATOMIC);
        assert(ib_res.readbuf_mr);
        assert(ib_res.recvbuf_mr);
    }


    printf("CREATE COMPLETION QUEUES...\n");
    ib_res.channel = ibv_create_comp_channel(ib_res.ctx);
    assert(ib_res.channel);
    ib_res.send_cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe, NULL, NULL, 0);
    ib_res.recv_cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe, NULL, ib_res.channel, 0);
    assert(ib_res.send_cq);
    assert(ib_res.recv_cq);

    printf("CREATE SHARED RECV QUEUE...\n");
    struct ibv_srq_init_attr srq_init_attr;
    memset(&srq_init_attr, 0, sizeof(struct ibv_srq_init_attr));
    // the maximum number of outstanding work requests that can be posted to this shared receive queue
    srq_init_attr.attr.max_wr = 64;
    // the maximum number of scatter/gather elements in any work request that can be posted to this srq
    srq_init_attr.attr.max_sge = 1;
    ib_res.srq = ibv_create_srq(ib_res.pd, &srq_init_attr);



    printf("CREATE QUEUE PAIRS...\n");
    struct ibv_qp_init_attr qp_init_attr;
    // for (int i = 0; i < SERVER_NUM;i++) {
    memset(&qp_init_attr, 0, sizeof(struct ibv_qp_init_attr));
    qp_init_attr.send_cq = ib_res.send_cq;
    qp_init_attr.recv_cq = ib_res.recv_cq;

    if (LOCAL_NODE == SERVER_NODE) {
        qp_init_attr.srq = ib_res.srq;
    }

    qp_init_attr.cap.max_recv_wr = ib_res.dev_attr.max_qp_wr;
    qp_init_attr.cap.max_send_wr = ib_res.dev_attr.max_qp_wr;
    qp_init_attr.cap.max_recv_sge = 3;
    qp_init_attr.cap.max_send_sge = 3;
    qp_init_attr.qp_type = IBV_QPT_RC;
    // the maximum message that can be posted inline to the send queue
    qp_init_attr.cap.max_inline_data = 512;

    // initiate qps for remote node
    // for server node: node_num * client_per_node qp should be prepared
    if (LOCAL_NODE == SERVER_NODE) {
        for (int i = 0;i < NODE_NUM;i++) {
            if (i == LOCAL_NODE) {
                continue;
            }
            for (int j = 0;j < CLIENT_PER_NODE;j++) {
                ib_res.server_qpset[i][j] = ibv_create_qp(ib_res.pd, &qp_init_attr);
                // ibInfo.QPSet[LOCAL_NODE][i] = ib_res.qp[i]->qp_num;
                assert(ib_res.server_qpset[i][j]);
            }
        }
    }
    else {
        for (int i = 0;i < CLIENT_PER_NODE;i++) {
            ib_res.client_qpset[i] = ibv_create_qp(ib_res.pd, &qp_init_attr);
        }
    }

    // record local lid and remote rkeys
    if (LOCAL_NODE == SERVER_NODE) {
        for (int i = 0;i < NODE_NUM;i++) {
            if (i == SERVER_NODE) {
                continue;
            }
            for (int j = 0;j < CLIENT_PER_NODE;j++) {
                ibInfoServer.QPSet[i][j] = ib_res.server_qpset[i][j]->qp_num;
            }
        }
        ibInfoServer.LidSet = ib_res.port_attr.lid;
        ibInfoServer.RKeySet = ib_res.mr->rkey;
    }
    else {
        for (int i = 0;i < CLIENT_PER_NODE;i++) {
            ibInfoClient.QPSet[LOCAL_NODE][i] = ib_res.client_qpset[i]->qp_num;
        }
        ibInfoClient.LidSet[LOCAL_NODE] = ib_res.port_attr.lid;
        ibInfoClient.RKeySet[LOCAL_NODE] = ib_res.recvbuf_mr->rkey;
    }

    ib_res.wc = (struct ibv_wc *)malloc(sizeof(struct ibv_wc));
    return 0;
}

int change_qp_state_rts() {
    printf("\n Modify QP State: \n");
    int ret;

    /* 4.1 change QP state to INIT */
    printf("CHANGE QP STATE TO INIT...\n");
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
        qp_attr.qp_state = IBV_QPS_INIT;
        qp_attr.pkey_index = 0;
        qp_attr.port_num = IB_PORT;
        qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                                  IBV_ACCESS_REMOTE_READ |
                                  IBV_ACCESS_REMOTE_ATOMIC |
                                  IBV_ACCESS_REMOTE_WRITE;

        if (LOCAL_NODE == SERVER_NODE) {
            for (int i = 0;i < NODE_NUM;i++) {
                if (i == LOCAL_NODE) {
                    continue;
                }
                for (int j = 0;j < CLIENT_PER_NODE;j++) {
                    ret = ibv_modify_qp(ib_res.server_qpset[i][j], &qp_attr,
                                        IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                                        IBV_QP_PORT  | IBV_QP_ACCESS_FLAGS);
                    assert(ret == 0);
                }
            }
        }
        else {
            for (int i = 0;i < CLIENT_PER_NODE;i++) {
                ret = ibv_modify_qp(ib_res.client_qpset[i], &qp_attr,
                                    IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                                    IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
                assert(ret == 0);
            }
        }
    }

    /* 4.2 change QP state to RTR */
    printf("CHANGE QP STATE TO RTR...\n");
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
        qp_attr.qp_state = IBV_QPS_RTR;
        qp_attr.path_mtu = IB_MTU;
        qp_attr.rq_psn = 0;
        qp_attr.max_dest_rd_atomic = 1;
        qp_attr.min_rnr_timer = 12;
        qp_attr.ah_attr.is_global = 0;
        qp_attr.ah_attr.sl = IB_SL;
        qp_attr.ah_attr.src_path_bits = 0;
        qp_attr.ah_attr.port_num = IB_PORT;

        if (LOCAL_NODE == SERVER_NODE) {
            for (int i = 0; i < NODE_NUM; i++) {
                if (i == LOCAL_NODE) {
                    continue;
                }
                for (int j = 0;j < CLIENT_PER_NODE;j++) {
                    qp_attr.dest_qp_num = ibInfoClient.QPSet[i][j];
                    qp_attr.ah_attr.dlid = ibInfoClient.LidSet[i];
                    ret = ibv_modify_qp(ib_res.server_qpset[i][j], &qp_attr,
                                        IBV_QP_STATE | IBV_QP_AV |
                                        IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                                        IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                                        IBV_QP_MIN_RNR_TIMER);
                    assert(ret == 0);
                }
            }
        }
        else {
            for (int i = 0; i < CLIENT_PER_NODE; i++) {
                qp_attr.dest_qp_num = ibInfoServer.QPSet[LOCAL_NODE][i];
                qp_attr.ah_attr.dlid = ibInfoServer.LidSet;
                ret = ibv_modify_qp(ib_res.client_qpset[i], &qp_attr,
                                    IBV_QP_STATE | IBV_QP_AV |
                                    IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                                    IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                                    IBV_QP_MIN_RNR_TIMER);
                assert(ret == 0);
            }
        }
    }

    /* 4.3 change QP state to RTS */
#ifdef MODULE_TEST
    printf("CHANGE QP STATE TO RTS...\n");
#endif
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
        qp_attr.qp_state = IBV_QPS_RTS;
        qp_attr.timeout = 10;
        qp_attr.retry_cnt = 7;
        qp_attr.rnr_retry = 7;
        qp_attr.sq_psn = 0;
        qp_attr.max_rd_atomic = 1;


        if (LOCAL_NODE == SERVER_NODE) {
            for (int i = 0; i < NODE_NUM; i++) {
                if (i == LOCAL_NODE) {
                    continue;
                }
                for (int j = 0;j < CLIENT_PER_NODE;j++) {
                    ret = ibv_modify_qp(ib_res.server_qpset[i][j], &qp_attr,
                                        IBV_QP_STATE | IBV_QP_TIMEOUT |
                                        IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                                        IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
                    assert(ret == 0);
                }
            }
        }
        else {
            for (int i = 0; i < CLIENT_PER_NODE; i++) {
                ret = ibv_modify_qp(ib_res.client_qpset[i], &qp_attr,
                                    IBV_QP_STATE | IBV_QP_TIMEOUT |
                                    IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                                    IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
                assert(ret == 0);
            }
        }
    }

    return 0;
}

int rdma_read(int client_order, char *r_addr, uint32_t size, char *l_addr) {
    int ret;

    struct ibv_sge read_sge;
    memset(&read_sge, 0, sizeof(struct ibv_sge));
    // read_sge.addr = (uintptr_t)(ib_res.recv_buf);
    read_sge.addr = (uintptr_t)(l_addr);
    read_sge.length = size;
    read_sge.lkey = ib_res.readbuf_mr->lkey;

    struct ibv_send_wr *bad_send_wr;
    struct ibv_send_wr send_wr;
    memset(&send_wr, 0, sizeof(struct ibv_send_wr));
    send_wr.wr_id = LOCAL_NODE;
    send_wr.sg_list = &read_sge;
    send_wr.num_sge = 1;
    send_wr.opcode = IBV_WR_RDMA_READ;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr.rdma.remote_addr = (uintptr_t)r_addr;
    send_wr.wr.rdma.rkey = ibInfoServer.RKeySet;

    ret = ibv_post_send(ib_res.client_qpset[client_order], &send_wr, &bad_send_wr);
    if(ret == -1) {
        return -1;
    }

    return 0;
}

int rdma_write(int client_order, char *r_addr, uint32_t size, char *l_addr, int flag, uint32_t imm) {
    int ret;
    struct ibv_sge write_sge;
    memset(&write_sge, 0, sizeof(struct ibv_sge));
    write_sge.addr = (uintptr_t)(l_addr);
    write_sge.length = size;
    write_sge.lkey = ib_res.recvbuf_mr->lkey;

    struct ibv_send_wr *bad_send_wr;

    struct ibv_send_wr send_wr;
    memset(&send_wr, 0, sizeof(struct ibv_send_wr));
    send_wr.wr_id = LOCAL_NODE;
    send_wr.sg_list = &write_sge;
    send_wr.num_sge = 1;
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr.rdma.remote_addr = (uintptr_t)(r_addr);
    send_wr.wr.rdma.rkey = ibInfoServer.RKeySet;
    switch(flag) {
        case 0:
            send_wr.send_flags |= IBV_SEND_INLINE;
            break;
        case 1:
            send_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
            send_wr.send_flags |= IBV_SEND_INLINE;
            send_wr.imm_data = imm;
            break;
        case 2:
            send_wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
            // need to wait signal for write
            send_wr.wr.atomic.remote_addr = (uintptr_t)r_addr;
            send_wr.wr.atomic.rkey = ibInfoServer.RKeySet;
            send_wr.wr.atomic.compare_add = 1;
    }

    ret = ibv_post_send(ib_res.client_qpset[client_order], &send_wr, &bad_send_wr);
    if (ret == -1) {
        return -1;
    }

    return 0;
}

int rdma_recv(int client_order) {
    printf("\nPost RDMA Recv: \n");
    int ret;
    struct ibv_recv_wr *bad_recv_wr;
    struct ibv_sge recv_sge;

    recv_sge.addr = 0;
    // in fact they are not used
    recv_sge.lkey = ib_res.recvbuf_mr->lkey;
    recv_sge.length = 4;

    struct ibv_recv_wr recv_wr;
    memset(&recv_wr, 0, sizeof(struct ibv_recv_wr));
    recv_wr.wr_id = LOCAL_NODE;
    recv_wr.num_sge = 1;
    recv_wr.sg_list = &recv_sge;

    ret = ibv_post_recv(ib_res.client_qpset[client_order], &recv_wr, &bad_recv_wr);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int rdma_srq_recv() {
    // printf("\nPost RDMA SRQ Recv: \n");
    int ret;
    struct ibv_recv_wr *bad_recv_wr;
    struct ibv_sge recv_sge;

    recv_sge.addr = 0;
    // in fact they are not used
    recv_sge.lkey = ib_res.mr->lkey;
    recv_sge.length = 8;

    struct ibv_recv_wr recv_wr;
    memset(&recv_sge, 0, sizeof(struct ibv_recv_wr));
    recv_wr.wr_id = LOCAL_NODE;
    recv_wr.num_sge = 1;
    recv_wr.sg_list = &recv_sge;

    ret = ibv_post_srq_recv(ib_res.srq, &recv_wr, &bad_recv_wr);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int post_cq(int flag, uint32_t &imm) {
    struct ibv_wc *wc = (struct ibv_wc *)malloc(sizeof(struct ibv_wc));
    struct ibv_cq *cq;
    // poll recv message
    if (flag == 0) {
        cq = ib_res.recv_cq;
    }
    // poll send message
    else {
        cq = ib_res.send_cq;
    }

    int n = 0;
    do {
        n = ibv_poll_cq(cq, 1, wc);
    } while (n == 0);

    // printf("%d CQE FOUND!\n", n);

    if (wc->status != IBV_WC_SUCCESS) {
        if (wc->opcode == IBV_WC_RDMA_WRITE) {
            printf("WC WRITE FAILED STATUS: %s.\n", ibv_wc_status_str(wc->status));
        } else if (wc->opcode == IBV_WC_RDMA_READ) {
            printf("WC READ FAILED STATUS: %s.\n", ibv_wc_status_str(wc->status));
        }
        else {
            printf("WC RECV FAILED STATUS: %s.\n", ibv_wc_status_str(wc->status));
        }
        free(wc);
        return -1;
    }

    if (flag == 0) {
        imm = wc->imm_data;
    }
    free(wc);
    return 0;
}

void close_ib() {
    printf("\nRelease IB resources: \n");
    int ret;

    if (LOCAL_NODE == SERVER_NODE) {
        for (int i = 0;i < NODE_NUM;i++) {
            if (i == LOCAL_NODE)
                continue;
            for (int j = 0;j < CLIENT_PER_NODE;j++) {
                ret = ibv_destroy_qp(ib_res.server_qpset[i][j]);
                assert(ret == 0);
                
            }
        }
    }

    ret = ibv_destroy_cq(ib_res.send_cq); assert(ret == 0);
    ret = ibv_destroy_cq(ib_res.recv_cq); assert(ret == 0);
    ret = ibv_destroy_comp_channel(ib_res.channel); assert(ret == 0);

    if (LOCAL_NODE == SERVER_NODE) {
        ret = ibv_dereg_mr(ib_res.mr); assert(ret == 0);
    }
    else {
        ret = ibv_dereg_mr(ib_res.recvbuf_mr); assert(ret == 0);
        ret = ibv_dereg_mr(ib_res.readbuf_mr); assert(ret == 0);
    }
    ret = ibv_dealloc_pd(ib_res.pd); assert(ret == 0);
    ret = ibv_close_device(ib_res.ctx); assert(ret == 0);
    ibv_free_device_list(device_list);
}
