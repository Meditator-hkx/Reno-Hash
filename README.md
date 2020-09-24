# Reno-Hash

Reno-Hash is an RDMA-enabled and NVM-optimized persistent hash table for fast remote access


# How to Use

## RDMA Support

The testing machines should be equipped with Infiniband card and (user-layer) Mellanox software packages should be installed.

## PM Support

The testing server should be equipped with NVM device, such as Intel Optane DC Persistent Memory. It should be configured as AppDirect Mode.

## Configure Environment

The basic configuration parameters are listed in the `config.h` and `config.cpp` files. Concretely, you should make the following modifications:

1. PM_PATH: the path of the DAX-enabled persistent file in your server.
2. NODE_NUM: number of nodes you want to test (nodes can be either virtual or actual).
3. ServerIPSet: the IP addresses of all nodes.
4. ServerPortSet: the port number used for the server to communicate with different clients.
5. ServerNodeSet: sequential number set for all the nodes.
6. LOCAL_NODE: current node number (in the machine).
7. SERVER_NODE: server node number for all nodes to know.
8. CLIENT_PER_NODE: how many clients there are in each client node.


## Compile and Try

`cd Reno-Hash`
`make`
`./reno (server-side)`
`./reno (client-side)`
