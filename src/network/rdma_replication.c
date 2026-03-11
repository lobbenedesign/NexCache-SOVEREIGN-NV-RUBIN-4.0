/* NexCache RDMA Replication Module (v5.0)
 * ============================================================
 * Implementazione logica per ibv_post_send / ibv_post_recv su
 * buffer MR pinnati. Simulata in ambiente POSIX per test.
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "rdma_replication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RdmaReplEngine *nex_rdma_init_engine(uint32_t self_node_id, const char *dev_name) {
    printf("[RDMA] Inizializzazione motore su device: %s (Nodo: %u)...\n",
           dev_name ? dev_name : "auto", self_node_id);

    RdmaReplEngine *e = calloc(1, sizeof(RdmaReplEngine));
    if (!e) return NULL;

    e->self_node_id = self_node_id;
    e->peer_count = 0;

    /* Allocazione buffer MR (Memory Region) da pin in RAM.
     * In un layer reale useremmo ibv_reg_mr() su questa memoria. */
    e->local_mr_buffer = calloc(1, RDMA_MR_SIZE);
    if (!e->local_mr_buffer) {
        free(e);
        return NULL;
    }
    e->write_offset = 0;

    printf("[RDMA] Memory Region (MR) allocata: %d MB. Kernel-bypass READY.\n",
           RDMA_MR_SIZE / (1024 * 1024));

    return e;
}

int nex_rdma_connect_peer(RdmaReplEngine *engine, const char *ip, int port, uint32_t node_id) {
    if (!engine || engine->peer_count >= RDMA_MAX_PEERS) return -1;

    printf("[RDMA] Handshake con Peer %u (%s:%d): Scambio rkey (Remote Key)...\n",
           node_id, ip, port);

    int idx = engine->peer_count++;
    engine->peers[idx].node_id = node_id;
    engine->peers[idx].is_connected = 1;

    /* Mock: asseganzione chiavi immaginarie */
    engine->peers[idx].remote_addr = 0x10000000;
    engine->peers[idx].rkey = 0xABCDEF;

    printf("[RDMA] Peer %u connesso. Scrittura diretta (Zero-CPU) abilitata.\n", node_id);
    return 0;
}

int nex_rdma_write_delta(RdmaReplEngine *engine, uint32_t target_node_id, const void *delta_payload, size_t len) {
    if (!engine || !delta_payload || len == 0) return -1;

    /* Trova il peer */
    int peer_idx = -1;
    for (int i = 0; i < engine->peer_count; i++) {
        if (engine->peers[i].node_id == target_node_id && engine->peers[i].is_connected) {
            peer_idx = i;
            break;
        }
    }

    if (peer_idx == -1) {
        printf("[RDMA ERROR] Peer %u non trovato o disconnesso.\n", target_node_id);
        return -1;
    }

    /* In uno stack reale:
     * ibv_sge list; set list.addr, list.length, list.lkey...
     * ibv_send_wr wr;
     * wr.opcode = IBV_WR_RDMA_WRITE;
     * wr.wr.rdma.remote_addr = engine->peers[peer_idx].remote_addr;
     * wr.wr.rdma.rkey = engine->peers[peer_idx].rkey;
     * ibv_post_send(qp, &wr, &bad_wr);
     */

    /* Stampa di log (in produzione sarebbe sostituita da trace o disabilitata) */
    /* printf("[RDMA TX] Invio RDMA WRITE (len: %zu) a peer %u. Zero-CPU.\n", len, target_node_id); */

    return 0;
}

int nex_rdma_poll_local_deltas(RdmaReplEngine *engine, void **out_payloads, int max_pkts) {
    if (!engine) return 0;

    /* Legge buffer ad anello locale su cui i peer mappati RDMA scrivono in modo ignorato dal kernel.
     * Raccoglie i delta in out_payloads asincronamente.
     */

    /* Non implementato per il simulatore */
    return 0;
}

void nex_rdma_destroy_engine(RdmaReplEngine *engine) {
    if (!engine) return;
    free(engine->local_mr_buffer);
    free(engine);
    printf("[RDMA] Motore RDMA e Memory Regions disallocati.\n");
}
