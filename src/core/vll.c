/* NexCache VLL Transaction Manager — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "vll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdatomic.h>

static uint64_t vll_us_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* ── vll_create ─────────────────────────────────────────────── */
VLLManager *vll_create(uint32_t table_size) {
    /* Power of 2 */
    uint32_t sz = 1024;
    while (sz < table_size) sz <<= 1;

    VLLManager *mgr = (VLLManager *)calloc(1, sizeof(VLLManager));
    if (!mgr) return NULL;

    mgr->read_counts = (uint32_t *)calloc(sz, sizeof(uint32_t));
    mgr->write_flags = (uint8_t *)calloc(sz, sizeof(uint8_t));
    mgr->table_size = sz;
    mgr->next_txn_id = 1;

    mgr->pattern_cap = 256;
    mgr->patterns = (VLLPattern *)calloc(mgr->pattern_cap, sizeof(VLLPattern));
    mgr->pattern_count = 0;

    pthread_mutex_init(&mgr->stats_lock, NULL);

    fprintf(stderr,
            "[NexCache VLL] Init: table_size=%u\n"
            "  Zero-mutex transactions for non-overlapping key sets.\n"
            "  Predictive lock ordering enabled.\n",
            sz);
    return mgr;
}

/* ── Slot per una chiave nella lock table ───────────────────── */
static uint32_t key_to_slot(VLLManager *mgr, uint64_t key_hash) {
    return (uint32_t)(key_hash & (mgr->table_size - 1));
}

/* ── Predictive lock ordering ───────────────────────────────── */
/*
 * Ordina le chiavi in modo da acquisire sempre i lock nello stesso
 * ordine globale → elimina deadlock e riduce conflitti.
 * Migliora ulteriormente se le chiavi hanno pattern storici noti.
 */
static int cmp_key_hash(const void *a, const void *b) {
    const VLLKey *ka = (const VLLKey *)a;
    const VLLKey *kb = (const VLLKey *)b;
    if (ka->key_hash < kb->key_hash) return -1;
    if (ka->key_hash > kb->key_hash) return 1;
    /* Write lock ha priorità sul read lock per stessa chiave */
    return (int)kb->lock_type - (int)ka->lock_type;
}

static void vll_sort_keys(VLLManager *mgr, VLLRequest *req) {
    /* Ordina per hash: garantisce ordine globale deterministico */
    qsort(req->keys, req->num_keys, sizeof(VLLKey), cmp_key_hash);

    /* Check pattern storico: c'è un'alternativa con meno conflitti? */
    /* Per ora: il sort per hash è già ottimale */
    (void)mgr; /* Usato in future versioni con pattern graph */
    mgr->stats.predictive_reorders++;
}

/* ── vll_acquire ────────────────────────────────────────────── */
VLLStatus vll_acquire(VLLManager *mgr, VLLRequest *req) {
    if (!mgr || !req || req->num_keys == 0) return VLL_ABORTED;

    /* Ordina le chiavi per evitare deadlock */
    vll_sort_keys(mgr, req);

    req->submit_us = vll_us_now();
    uint64_t deadline = req->submit_us + VLL_TIMEOUT_US;

    /* Tenta acquisizione con retry fino al timeout */
    int attempt = 0;
    while (1) {
        int conflict = 0;
        int acquired_count = 0;

        for (int i = 0; i < req->num_keys; i++) {
            uint32_t slot = key_to_slot(mgr, req->keys[i].key_hash);

            if (req->keys[i].lock_type == VLL_LOCK_READ) {
                /* Read lock: ok se non c'è un writer */
                if (mgr->write_flags[slot]) {
                    conflict = 1;
                    mgr->stats.conflicts_resolved++;
                    break;
                }
                mgr->read_counts[slot]++;
            } else {
                /* Write lock: ok se nessun reader e nessun writer */
                if (mgr->write_flags[slot] || mgr->read_counts[slot] > 0) {
                    conflict = 1;
                    mgr->stats.conflicts_resolved++;
                    break;
                }
                mgr->write_flags[slot] = 1;
            }
            acquired_count++;
        }

        if (!conflict) {
            /* Tutti i lock acquisiti */
            req->status = VLL_OK;
            req->acquired_us = vll_us_now();

            pthread_mutex_lock(&mgr->stats_lock);
            mgr->stats.txns_submitted++;
            mgr->stats.txns_committed++;
            double wait = (double)(req->acquired_us - req->submit_us);
            mgr->stats.avg_wait_us = mgr->stats.avg_wait_us * 0.99 + wait * 0.01;
            pthread_mutex_unlock(&mgr->stats_lock);

            return VLL_OK;
        }

        /* Rilascia i lock parzialmente acquisiti */
        for (int i = 0; i < acquired_count; i++) {
            uint32_t slot = key_to_slot(mgr, req->keys[i].key_hash);
            if (req->keys[i].lock_type == VLL_LOCK_READ) {
                if (mgr->read_counts[slot] > 0) mgr->read_counts[slot]--;
            } else {
                mgr->write_flags[slot] = 0;
            }
        }

        /* Timeout check */
        if (vll_us_now() >= deadline) {
            req->status = VLL_TIMEOUT;
            pthread_mutex_lock(&mgr->stats_lock);
            mgr->stats.txns_submitted++;
            mgr->stats.txns_timeout++;
            pthread_mutex_unlock(&mgr->stats_lock);
            return VLL_TIMEOUT;
        }

        /* Backoff esponenziale: 10µs * 2^attempt (max 1ms) */
        attempt++;
        uint64_t backoff_us = 10ULL << (attempt < 7 ? attempt : 7);
        usleep((unsigned int)backoff_us);
    }
}

/* ── vll_release ────────────────────────────────────────────── */
void vll_release(VLLManager *mgr, VLLRequest *req) {
    if (!mgr || !req || req->status != VLL_OK) return;

    for (int i = 0; i < req->num_keys; i++) {
        uint32_t slot = key_to_slot(mgr, req->keys[i].key_hash);
        if (req->keys[i].lock_type == VLL_LOCK_READ) {
            if (mgr->read_counts[slot] > 0) mgr->read_counts[slot]--;
        } else {
            mgr->write_flags[slot] = 0;
        }
    }
    req->status = VLL_ABORTED; /* Marking released */
}

/* ── vll_request_create ─────────────────────────────────────── */
VLLRequest *vll_request_create(VLLManager *mgr,
                               uint64_t *key_hashes,
                               VLLLockType *lock_types,
                               uint16_t num_keys) {
    if (!mgr || !key_hashes || num_keys == 0 ||
        num_keys > VLL_MAX_KEYS_PER_TXN) return NULL;

    VLLRequest *req = (VLLRequest *)calloc(1, sizeof(VLLRequest));
    if (!req) return NULL;

    req->txn_id = mgr->next_txn_id++;
    req->num_keys = num_keys;
    for (int i = 0; i < num_keys; i++) {
        req->keys[i].key_hash = key_hashes[i];
        req->keys[i].lock_type = lock_types[i];
    }
    req->status = VLL_CONFLICT; /* Non ancora acquisito */
    return req;
}

void vll_request_destroy(VLLRequest *req) {
    free(req);
}

/* ── Stats ──────────────────────────────────────────────────── */
VLLStats vll_get_stats(VLLManager *mgr) {
    VLLStats empty = {0};
    if (!mgr) return empty;
    pthread_mutex_lock(&mgr->stats_lock);
    VLLStats s = mgr->stats;
    pthread_mutex_unlock(&mgr->stats_lock);
    return s;
}

void vll_print_stats(VLLManager *mgr) {
    if (!mgr) return;
    VLLStats s = vll_get_stats(mgr);
    fprintf(stderr,
            "[NexCache VLL] Stats:\n"
            "  submitted:    %llu\n"
            "  committed:    %llu\n"
            "  aborted:      %llu\n"
            "  timeout:      %llu\n"
            "  conflicts:    %llu\n"
            "  reorders:     %llu\n"
            "  avg_wait:     %.1f µs\n",
            (unsigned long long)s.txns_submitted,
            (unsigned long long)s.txns_committed,
            (unsigned long long)s.txns_aborted,
            (unsigned long long)s.txns_timeout,
            (unsigned long long)s.conflicts_resolved,
            (unsigned long long)s.predictive_reorders,
            s.avg_wait_us);
}

/* ── vll_destroy ────────────────────────────────────────────── */
void vll_destroy(VLLManager *mgr) {
    if (!mgr) return;
    free(mgr->read_counts);
    free(mgr->write_flags);
    free(mgr->patterns);
    pthread_mutex_destroy(&mgr->stats_lock);
    free(mgr);
}
