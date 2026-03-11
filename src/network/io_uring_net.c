#include "net_backend.h"

/*
 * Stub implementation of io_uring Network Backend
 * 100-200us latencies. Zero-copy.
 */

static int io_uring_init(const char *interface, int num_queues) {
    (void)interface;
    (void)num_queues;
    return 0; /* OK */
}

static int io_uring_recv(struct NetPacket **pkts, int max_pkts) {
    (void)pkts;
    (void)max_pkts;
    return 0;
}

static int io_uring_send(struct NetPacket **pkts, int n_pkts) {
    (void)pkts;
    (void)n_pkts;
    return 0;
}

static void io_uring_free_pkt(struct NetPacket *pkt) {
    (void)pkt;
}

static void io_uring_stats(struct NetStats *out) {
    if (out) {
        out->pkts_rx = 0;
        out->pkts_tx = 0;
        out->bytes_rx = 0;
        out->bytes_tx = 0;
        out->drops = 0;
    }
}

const NetBackend IoUringBackend = {
    .name = "io_uring",
    .init = io_uring_init,
    .recv = io_uring_recv,
    .send = io_uring_send,
    .free_pkt = io_uring_free_pkt,
    .stats = io_uring_stats};
