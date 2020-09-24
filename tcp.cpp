/* A simple server in the internet domain using TCP
   The port number is passed as an argument */

#include "tcp.h"
#include <errno.h>

// ExchangeInfo remote_qp_info;

int buildServer() {
    int sockfd, newsockfd;
    int portno, clilen;
    SockArrary sockArrary;
    sockArrary.sock_num = 0;
    int client_node_num = 0;

    struct sockaddr_in serv_addr, cli_addr;
    int ret, n;


    setup_ib();

    // 1. create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("buildServer error: fail to create the socket.\n");
        return -1;
    }
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    portno = ServerPortSet[SERVER_NODE];

    // 2. bind socket
    printf("BIND SOCKET.\n");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        printf("buildServer error: fail to bind the socket.\n");
        return -1;
    }

    // 3. listen with socket
    printf("SOCKET LISTENNING.\n");
    listen(sockfd, 5);

    // 4. accept client connection
    ClientSendInfo *recvBuf = (ClientSendInfo *)malloc(sizeof(ClientSendInfo));
    // continue the loop until all slave nodes connect to master
    while (client_node_num < NODE_NUM - 1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
        if (ret < 0) {
            printf("buildServer error: fail to accept the client connection.\n");
            return -1;
        }

        // 5. exchange information
        // 5.1 read client info from buffer
        n = sock_read(newsockfd, recvBuf, sizeof(ClientSendInfo));
        if (n < 0) {
            printf("buildServer error: fail to receive client data.\n");
            return -1;
        }
        else {
            // store client information
            uint32_t node = recvBuf->node;
            sockArrary.socket[client_node_num] = newsockfd;
            sockArrary.clientnode[client_node_num] = node;
            sockArrary.sock_num++;

            // record client information in global IB structure
            for (int i = 0;i < CLIENT_PER_NODE;i++) {
                ibInfoClient.QPSet[node][i] = recvBuf->ibInfoClient.QPSet[node][i];
            }
            ibInfoClient.LidSet[node] = recvBuf->ibInfoClient.LidSet[node];
            ibInfoClient.RKeySet[node] = recvBuf->ibInfoClient.RKeySet[node];
        }
        client_node_num++;
    }

    // 5.2 write server info to buffer
    ServerSendInfo *sendBuf = (ServerSendInfo *)malloc(sizeof(ServerSendInfo));
    int node;
    for (int i = 0;i < sockArrary.sock_num;i++) {
        newsockfd = sockArrary.socket[i];

        // fill in send buffer
        node = sockArrary.clientnode[i];
        memcpy(&sendBuf->ibInfoServer, &ibInfoServer, sizeof(IBInfo_Server));
        sendBuf->Super = (uint64_t)Super;
        sendBuf->index_num = Super->iline_num;
        sendBuf->data_index_addr = (uint64_t)ILine;
        sendBuf->data_line_addr = (uint64_t)DLine;
        n = sock_write(newsockfd, sendBuf, sizeof(ServerSendInfo));
        if (n < 0) {
            printf("buildServer error: fail to send server data.\n");
            return -1;
        }
    }

    close(sockfd);

    // 5. Initiate all QP pairs (to remote nodes)
    ret = change_qp_state_rts();
    if (ret == -1) {
        printf("buildServer error: fail to initiate all QP states to connect remote servers.\n");
        return -1;
    }
    return 0;
}

int buildClient() {
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;
    int ret, n;

    setup_ib();

    // 1. create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("buildClient error: fail to create the socket.\n");
        return -1;
    }

    // 2. get server information
    server = gethostbyname(ServerIPSet[SERVER_NODE]);
    if (server == NULL) {
        printf("buildClient error: fail to obtain the server information.\n");
        return -1;
    }
    bzero((char *)&server_addr, sizeof(server_addr));
    // memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    // memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(ServerPortSet[SERVER_NODE]);

    // 3. connect to server
    ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("buildClient error: fail to connect to server.\n");
        return -1;
    }

    // 4. exchange information
    // 4.1 write client info to buffer
    ClientSendInfo *sendBuf = (ClientSendInfo *)malloc(sizeof(ClientSendInfo));
    sendBuf->node = LOCAL_NODE;
    memcpy(&sendBuf->ibInfoClient, &ibInfoClient, sizeof(IBInfo_Client));

    n = sock_write(sockfd, sendBuf, sizeof(ClientSendInfo));
    if (n < 0) {
        printf("buildClient error: fail to send client data.\n");
        return -1;
    }
    // 4.2 read server info in buffer
    ServerSendInfo *recvBuf = (ServerSendInfo *)malloc(sizeof(ServerSendInfo));
    n = sock_read(sockfd, recvBuf, sizeof(ServerSendInfo));
    if (n < 0) {
        printf("buildClient error: fail to receive server data.\n");
        return -1;
    }
    else {
        // update local ibInfo
        memcpy(&ibInfoServer, &recvBuf->ibInfoServer, sizeof(IBInfo_Server));

        // local save for hash & remote access
        Super = (SuperMeta *)recvBuf->Super;
        IndexLineNum = recvBuf->index_num;
        ILine = (IndexLine *)recvBuf->data_index_addr;
        DLine = (DataLine *)recvBuf->data_line_addr;
    }

    // 5. Initiate all QP pairs (to remote nodes)
    ret = change_qp_state_rts();
    if (ret == -1) {
        printf("buildClient error: fail to initiate all QP states to connect remote servers.\n");
        return -1;
    }
    return 0;
}


int sock_read(int sockfd, void *buffer, uint32_t len) {
    uint32_t nr, total_read;
    char *buf = (char *)buffer;
    total_read = 0;
    while (len != 0) {
        nr = read(sockfd, buf, len);
        if (nr < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                return -1;
            }
        }
        if (nr == 0) {
            break;
        }
        else {
            len -= nr;
            buf += nr;
            total_read += nr;
        }
    }

    return total_read;
}

int sock_write(int sockfd, void *buffer, uint32_t len) {
    uint32_t nw, total_written;
    const char *buf = (char *)buffer;
    total_written = 0;
    while (len != 0) {
        nw = write(sockfd, buf, len);
        if (nw < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                return -1;
            }
        }
        if (nw == 0) {
            break;
        }
        else {
            len -= nw;
            buf += nw;
            total_written += nw;
        }
    }

    return total_written;
}
