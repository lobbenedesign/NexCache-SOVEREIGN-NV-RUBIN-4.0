#ifndef NEXCACHE_NET_BACKEND_H
#define NEXCACHE_NET_BACKEND_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Network Packet wrapper
 */
typedef struct NetPacket {
    void *data;
    size_t length;
    int socket_id; /* Generic integer for connection context or desc id */
    /* ... more fields depending on implementation */
} NetPacket;

/**
 * @brief Network Statistics
 */
typedef struct NetStats {
    uint64_t pkts_rx;
    uint64_t pkts_tx;
    uint64_t bytes_rx;
    uint64_t bytes_tx;
    uint64_t drops;
} NetStats;

/**
 * @brief Unified interface for Network Layer (DPDK, io_uring, epoll)
 */
typedef struct NetBackend {
    const char *name; /* e.g., "dpdk", "io_uring", "epoll" */

    /**
     * @brief Initialize the network backend
     * @param interface Network interface name (e.g., "eth0" or "0000:01:00.0" for DPDK)
     * @param num_queues Number of network queues
     * @return 0 on success, < 0 on error
     */
    int (*init)(const char *interface, int num_queues);

    /**
     * @brief Receive packets from network. Non-blocking.
     * @param pkts Array of packet pointers to populate
     * @param max_pkts Max packets to receive
     * @return Number of packets received
     */
    int (*recv)(struct NetPacket **pkts, int max_pkts);

    /**
     * @brief Send packets out.
     * @param pkts Array of packet pointers to send
     * @param n_pkts Number of packets in array
     * @return Number of packets successfully sent
     */
    int (*send)(struct NetPacket **pkts, int n_pkts);

    /**
     * @brief Free memory associated with a packet wrapper
     */
    void (*free_pkt)(struct NetPacket *pkt);

    /**
     * @brief Get network statistics
     */
    void (*stats)(struct NetStats *out);
} NetBackend;

extern const NetBackend DPDKBackend;
extern const NetBackend IoUringBackend;
extern const NetBackend EpollBackend;

#endif /* NEXCACHE_NET_BACKEND_H */
