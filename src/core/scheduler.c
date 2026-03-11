/* NexCache Work Stealing Scheduler — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* ── Utility tempo ──────────────────────────────────────────── */
static uint64_t sched_us_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* ── scheduler_create ───────────────────────────────────────── */
NexScheduler *scheduler_create(NexEngine *engine) {
    if (!engine) return NULL;

    NexScheduler *sched = (NexScheduler *)calloc(1, sizeof(NexScheduler));
    if (!sched) return NULL;

    sched->engine = engine;
    sched->enabled = 1;

    for (int i = 0; i < engine->num_workers; i++) {
        WorkerSchedState *ws = &sched->states[i];
        atomic_init(&ws->queue_depth, 0);
        atomic_init(&ws->steal_attempts, 0);
        atomic_init(&ws->steal_successes, 0);
        atomic_init(&ws->stolen_from_me, 0);
        atomic_init(&ws->is_idle, 0);
        ws->utilization = 0.0;
        ws->last_steal_us = 0;
    }

    atomic_init(&sched->total_steals, 0);
    atomic_init(&sched->total_steal_fails, 0);
    sched->rebalances = 0;
    sched->avg_load_balance = 0.0;

    fprintf(stderr, "[NexCache Scheduler] Work Stealing enabled for %d workers\n",
            engine->num_workers);
    return sched;
}

/* ── scheduler_enable ───────────────────────────────────────── */
void scheduler_enable(NexScheduler *sched, int enable) {
    if (!sched) return;
    sched->enabled = enable;
    fprintf(stderr, "[NexCache Scheduler] Work stealing %s\n",
            enable ? "ENABLED" : "DISABLED");
}

/* ── scheduler_try_steal — Power-of-Two-Choices ────────────── */
/*
 * Algoritmo Power-of-Two-Choices (Mitzenmacher 1996):
 * Invece di cercare il worker più carico (O(N) scan),
 * campioniamo DUE worker casuali e rubiamo dal più carico dei due.
 * Risultato: O(log log N) bilanciamento con O(1) overhead.
 *
 * Algoritmo alternativo per piccoli cluster (< 8 workers):
 * scan completo — sempre O(N) ma N piccolo.
 */
int scheduler_try_steal(NexScheduler *sched, int my_worker_id) {
    if (!sched || !sched->enabled) return 0;

    NexEngine *engine = sched->engine;
    int n = engine->num_workers;
    if (n <= 1) return 0;

    uint64_t now = sched_us_now();
    WorkerSchedState *my_state = &sched->states[my_worker_id];

    /* Evita furto troppo frequente (backoff esponenziale) */
    if (now - my_state->last_steal_us < SCHED_STEAL_CHECK_US) {
        return 0;
    }

    atomic_fetch_add(&my_state->steal_attempts, 1);

    int victim_id = -1;
    uint64_t max_depth = 0;

    if (n <= 8) {
        /* Scan completo per cluster piccoli */
        for (int i = 0; i < n; i++) {
            if (i == my_worker_id) continue;
            uint64_t depth = atomic_load_explicit(
                &sched->states[i].queue_depth, memory_order_relaxed);
            if (depth > SCHED_STEAL_THRESHOLD && depth > max_depth) {
                max_depth = depth;
                victim_id = i;
            }
        }
    } else {
        /* Power-of-Two-Choices: campiona 2 worker casuali */
        /* Usa timestamp come seed per evitare dipendenza da rand() */
        int c1 = (int)((now >> 4) % (uint64_t)(n - 1));
        int c2 = (int)((now >> 8) % (uint64_t)(n - 1));
        if (c1 >= my_worker_id) c1++;
        if (c2 >= my_worker_id) c2++;
        if (c2 == c1) c2 = (c2 + 1) % n;
        if (c2 == my_worker_id) c2 = (c2 + 1) % n;

        uint64_t d1 = atomic_load_explicit(
            &sched->states[c1].queue_depth, memory_order_relaxed);
        uint64_t d2 = atomic_load_explicit(
            &sched->states[c2].queue_depth, memory_order_relaxed);

        if (d1 >= d2 && d1 > SCHED_STEAL_THRESHOLD) {
            victim_id = c1;
            max_depth = d1;
        } else if (d2 > SCHED_STEAL_THRESHOLD) {
            victim_id = c2;
            max_depth = d2;
        }
    }

    if (victim_id < 0) {
        atomic_fetch_add(&sched->total_steal_fails, 1);
        return 0;
    }

    /* Tentativo di furto: prende SCHED_STEAL_BATCH_SIZE comandi */
    NexWorker *victim = &engine->workers[victim_id];
    NexWorker *stealer = &engine->workers[my_worker_id];
    int stolen = 0;

    for (int s = 0; s < SCHED_STEAL_BATCH_SIZE; s++) {
        /* Leggi dalla TESTA della coda della vittima (opposta alla CODA
         * che la vittima usa — evita contesa senza lock) */
        uint64_t v_tail = atomic_load_explicit(
            &victim->cmd_tail, memory_order_acquire);
        uint64_t v_head = atomic_load_explicit(
            &victim->cmd_head, memory_order_relaxed);

        if (v_head <= v_tail + SCHED_STEAL_THRESHOLD) {
            break; /* Non abbastanza lavoro da rubare */
        }

        /* Prova a incrementare il tail della vittima (furto dalla testa) */
        NexCmd *cmd = victim->cmd_ring[v_tail & NEX_CMD_RING_MASK];
        if (!cmd) break;

        /* CAS atomic: solo un thread alla volta ruba questa entry */
        uint64_t expected_tail = v_tail;
        if (!atomic_compare_exchange_weak_explicit(
                &victim->cmd_tail,
                &expected_tail,
                v_tail + 1,
                memory_order_acq_rel,
                memory_order_relaxed)) {
            break; /* Un altro thread ha già rubato questa entry */
        }

        /* Inserisci nella coda dello stealer */
        uint64_t my_head = atomic_load_explicit(
            &stealer->cmd_head, memory_order_relaxed);
        stealer->cmd_ring[my_head & NEX_CMD_RING_MASK] = cmd;
        atomic_store_explicit(&stealer->cmd_head, my_head + 1,
                              memory_order_release);
        stolen++;
    }

    if (stolen > 0) {
        atomic_fetch_add(&my_state->steal_successes, 1);
        atomic_fetch_add(&sched->states[victim_id].stolen_from_me, stolen);
        atomic_fetch_add(&sched->total_steals, stolen);
        my_state->last_steal_us = now;
        sched->rebalances++;
    } else {
        atomic_fetch_add(&sched->total_steal_fails, 1);
    }

    my_state->last_steal_us = now;
    return stolen;
}

/* ── scheduler_update_utilization ──────────────────────────── */
void scheduler_update_utilization(NexScheduler *sched,
                                  int worker_id,
                                  double util) {
    if (!sched || worker_id < 0 || worker_id >= sched->engine->num_workers) return;

    WorkerSchedState *ws = &sched->states[worker_id];
    ws->utilization = util;

    /* Aggiorna queue depth basandosi sul worker */
    NexWorker *w = &sched->engine->workers[worker_id];
    uint64_t head = atomic_load_explicit(&w->cmd_head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&w->cmd_tail, memory_order_relaxed);
    uint64_t depth = (head >= tail) ? (head - tail) : 0;
    atomic_store_explicit(&ws->queue_depth, depth, memory_order_relaxed);

    /* Segna idle se utilization < 5% */
    atomic_store_explicit(&ws->is_idle, (util < 0.05) ? 1 : 0,
                          memory_order_relaxed);
}

/* ── scheduler_print_stats ──────────────────────────────────── */
void scheduler_print_stats(NexScheduler *sched) {
    if (!sched) return;

    fprintf(stderr, "[NexCache Scheduler Stats]\n");
    fprintf(stderr, "  Work stealing: %s\n",
            sched->enabled ? "ENABLED" : "DISABLED");
    fprintf(stderr, "  Total steals:  %llu\n",
            (unsigned long long)atomic_load(&sched->total_steals));
    fprintf(stderr, "  Steal fails:   %llu\n",
            (unsigned long long)atomic_load(&sched->total_steal_fails));
    fprintf(stderr, "  Rebalances:    %llu\n",
            (unsigned long long)sched->rebalances);

    double total_util = 0.0;
    double max_util = 0.0;
    int n = sched->engine->num_workers;

    for (int i = 0; i < n; i++) {
        WorkerSchedState *ws = &sched->states[i];
        total_util += ws->utilization;
        if (ws->utilization > max_util) max_util = ws->utilization;
        fprintf(stderr,
                "  Worker[%2d] util=%.0f%% qdepth=%llu "
                "steals=%llu stolen_from=%llu idle=%d\n",
                i,
                ws->utilization * 100.0,
                (unsigned long long)atomic_load(&ws->queue_depth),
                (unsigned long long)atomic_load(&ws->steal_successes),
                (unsigned long long)atomic_load(&ws->stolen_from_me),
                atomic_load(&ws->is_idle));
    }

    double avg_util = n > 0 ? total_util / n : 0.0;
    double imbalance = n > 0 ? (max_util - avg_util) / (max_util + 0.001) : 0.0;
    fprintf(stderr,
            "  Load balance: avg=%.0f%% max=%.0f%% imbalance=%.1f%%\n",
            avg_util * 100.0, max_util * 100.0, imbalance * 100.0);
}

/* ── scheduler_destroy ──────────────────────────────────────── */
void scheduler_destroy(NexScheduler *sched) {
    if (!sched) return;
    free(sched);
}
