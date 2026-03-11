/* NexCache Hazard Pointers — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "hazard_ptr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ── Tabella globale hazard records ─────────────────────────── */
static HazardRecord g_records[HP_MAX_THREADS];
static _Atomic int g_initialized = 0;
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Thread-local state ─────────────────────────────────────── */
static _Thread_local HazardState tl_state;
static _Thread_local int tl_registered = 0;

/* ── hp_init ────────────────────────────────────────────────── */
int hp_init(void) {
    pthread_mutex_lock(&g_init_lock);
    if (atomic_load(&g_initialized)) {
        pthread_mutex_unlock(&g_init_lock);
        return 0;
    }
    memset(g_records, 0, sizeof(g_records));
    for (int i = 0; i < HP_MAX_THREADS; i++) {
        for (int j = 0; j < HP_PER_THREAD; j++) {
            atomic_init(&g_records[i].hp[j], NULL);
        }
        atomic_init(&g_records[i].active, 0);
        g_records[i].thread_id = -1;
    }
    atomic_store(&g_initialized, 1);
    pthread_mutex_unlock(&g_init_lock);
    return 0;
}

/* ── hp_shutdown ────────────────────────────────────────────── */
void hp_shutdown(void) {
    /* Forza scan di tutti i retire list pendenti */
    /* In produzione si itererebbe su tutti i thread registrati */
    atomic_store(&g_initialized, 0);
}

/* ── hp_thread_init ─────────────────────────────────────────── */
int hp_thread_init(void) {
    if (tl_registered) return 0;

    /* Trova un slot libero nella tabella globale */
    for (int i = 0; i < HP_MAX_THREADS; i++) {
        int expected = 0;
        if (atomic_compare_exchange_strong(&g_records[i].active, &expected, 1)) {
            tl_state.my_record = &g_records[i];
            tl_state.retire_list = NULL;
            tl_state.retire_count = 0;
            g_records[i].thread_id = i;
            tl_registered = 1;
            return 0;
        }
    }
    fprintf(stderr, "[NexCache HP] FATAL: HP_MAX_THREADS (%d) exceeded\n",
            HP_MAX_THREADS);
    return -1;
}

/* ── hp_thread_exit ─────────────────────────────────────────── */
void hp_thread_exit(void) {
    if (!tl_registered) return;
    hp_release_all();
    hp_scan(); /* Processa tutti i retire list pendenti */
    atomic_store(&tl_state.my_record->active, 0);
    tl_registered = 0;
}

/* ── hp_acquire ─────────────────────────────────────────────── */
void hp_acquire(int slot, void *ptr) {
    if (!tl_registered || slot < 0 || slot >= HP_PER_THREAD) return;
    atomic_store_explicit(&tl_state.my_record->hp[slot],
                          ptr,
                          memory_order_release);
    /* barrier: assicura che lo store sia visibile prima di continuare */
    atomic_thread_fence(memory_order_seq_cst);
}

/* ── hp_release ─────────────────────────────────────────────── */
void hp_release(int slot) {
    if (!tl_registered || slot < 0 || slot >= HP_PER_THREAD) return;
    atomic_store_explicit(&tl_state.my_record->hp[slot],
                          NULL,
                          memory_order_release);
}

/* ── hp_release_all ─────────────────────────────────────────── */
void hp_release_all(void) {
    if (!tl_registered) return;
    for (int i = 0; i < HP_PER_THREAD; i++) {
        atomic_store_explicit(&tl_state.my_record->hp[i],
                              NULL,
                              memory_order_release);
    }
}

/* ── hp_retire ──────────────────────────────────────────────── */
void hp_retire(void *ptr, void (*free_fn)(void *)) {
    if (!ptr) return;
    if (!tl_registered) hp_thread_init(); /* Auto-init in emergenza */

    RetiredPtr *rp = (RetiredPtr *)malloc(sizeof(RetiredPtr));
    if (!rp) {
        /* Last resort: dealloca subito (possibilmente unsafe) */
        if (free_fn)
            free_fn(ptr);
        else
            free(ptr);
        return;
    }
    rp->ptr = ptr;
    rp->free_fn = free_fn;
    rp->next = tl_state.retire_list;
    tl_state.retire_list = rp;
    tl_state.retire_count++;

    if (tl_state.retire_count >= HP_RETIRE_THRESHOLD) {
        hp_scan();
    }
}

/* ── Raccolta puntatori hazardous da tutti i thread ──────────── */
static int collect_hazardous(void **out, int max) {
    int count = 0;
    for (int i = 0; i < HP_MAX_THREADS && count < max; i++) {
        if (!atomic_load(&g_records[i].active)) continue;
        for (int j = 0; j < HP_PER_THREAD && count < max; j++) {
            void *p = atomic_load_explicit(&g_records[i].hp[j],
                                           memory_order_acquire);
            if (p) out[count++] = p;
        }
    }
    return count;
}

/* ── hp_scan ────────────────────────────────────────────────── */
void hp_scan(void) {
    if (!tl_registered || !tl_state.retire_list) return;

    /* Raccogli tutti i puntatori hazardous correnti */
    void *hazardous[HP_MAX_THREADS * HP_PER_THREAD];
    int n_haz = collect_hazardous(hazardous, HP_MAX_THREADS * HP_PER_THREAD);

    /* Filtra la retire list: dealloca quelli non hazardous */
    RetiredPtr *kept = NULL;
    RetiredPtr *curr = tl_state.retire_list;
    int kept_count = 0;

    while (curr) {
        RetiredPtr *next = curr->next;
        int is_hazardous = 0;

        for (int i = 0; i < n_haz; i++) {
            if (hazardous[i] == curr->ptr) {
                is_hazardous = 1;
                break;
            }
        }

        if (is_hazardous) {
            /* Mantieni nella lista, riprova al prossimo scan */
            curr->next = kept;
            kept = curr;
            kept_count++;
        } else {
            /* Sicuro da deallocare */
            if (curr->free_fn) {
                curr->free_fn(curr->ptr);
            } else {
                free(curr->ptr);
            }
            free(curr);
        }
        curr = next;
    }

    tl_state.retire_list = kept;
    tl_state.retire_count = kept_count;
}
