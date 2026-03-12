#include "net_backend.h"

/*
 * Stub implementation of DPDK Network Backend
 * Represents kernel-bypass DPDK polling 10-20us latencies.
 */

static int dpdk_init(const char *interface, int num_queues) {
    (void)interface;
    (void)num_queues;
    return -1; /* DPDK not fully stubbed here */
}

static int dpdk_recv(struct NetPacket **pkts, int max_pkts) {
    (void)pkts;
    (void)max_pkts;
    return 0;
}

static int dpdk_send(struct NetPacket **pkts, int n_pkts) {
    (void)pkts;
    (void)n_pkts;
    return 0;
}

static void dpdk_free_pkt(struct NetPacket *pkt) {
    (void)pkt;
}

static void dpdk_stats(struct NetStats *out) {
    if (out) {
        out->pkts_rx = 0;
        out->pkts_tx = 0;
        out->bytes_rx = 0;
        out->bytes_tx = 0;
        out->drops = 0;
    }
}

const NetBackend DPDKBackend = {
    .name = "dpdk",
    .init = dpdk_init,
    .recv = dpdk_recv,
    .send = dpdk_send,
    .free_pkt = dpdk_free_pkt,
    .stats = dpdk_stats};
