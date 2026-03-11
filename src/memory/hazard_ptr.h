/* NexCache Hazard Pointers — Lock-free Memory Reclamation
 * ============================================================
 * Necessario per data structure lock-free sicure.
 *
 * Il problema: con strutture dati lock-free, un thread può leggere
 * un puntatore mentre un altro thread lo sta deallocando.
 *
 * Soluzione: Hazard Pointers (Michael, 2004).
 * Prima di dereferenziare un puntatore condiviso, il thread
 * "dichiara" il puntatore come "hazardous" (in uso).
 * Il garbage collector non dealloca puntatori hazardous.
 *
 * Reference: https://dl.acm.org/doi/10.1145/987524.987595
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_HAZARD_PTR_H
#define NEXCACHE_HAZARD_PTR_H

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

/* ── Costanti ───────────────────────────────────────────────── */
#define HP_MAX_THREADS 128     /* Thread massimi che usano HP */
#define HP_PER_THREAD 8        /* Hazard pointer per thread */
#define HP_RETIRE_THRESHOLD 64 /* Dealloca quando accumulated > N */

/* ── Array globale hazard pointer (read da tutti i thread) ─── */
/* Allineato a cache line per evitare false sharing */
typedef struct __attribute__((aligned(64))) HazardRecord {
    _Atomic(void *) hp[HP_PER_THREAD]; /* Puntatori hazardous */
    _Atomic int active;                /* 1 = thread attivo */
    int thread_id;                     /* ID thread proprietario */
    /* Padding a 128 byte per sicurezza false-sharing */
    char _pad[128 - sizeof(_Atomic(void *)) * HP_PER_THREAD - sizeof(_Atomic int) - sizeof(int)];
} HazardRecord;

/* ── Lista di puntatori da deallocare ───────────────────────── */
typedef struct RetiredPtr {
    void *ptr;
    void (*free_fn)(void *); /* Funzione di dealloc specifica */
    struct RetiredPtr *next;
} RetiredPtr;

/* ── Stato per thread (thread-local) ───────────────────────── */
typedef struct HazardState {
    HazardRecord *my_record; /* Record hazard di questo thread */
    RetiredPtr *retire_list; /* Lista puntatori da deallocare */
    int retire_count;        /* Numero nella lista */
} HazardState;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * hp_init - Inizializza il subsistema hazard pointer.
 * Returns: 0 su successo, -1 su errore.
 */
int hp_init(void);

/**
 * hp_shutdown - Libera tutte le risorse e dealloca puntatori pendenti.
 */
void hp_shutdown(void);

/**
 * hp_acquire - Dichiara un puntatore come hazardous (in uso).
 * @slot:   Slot da usare (0..HP_PER_THREAD-1)
 * @ptr:    Puntatore da proteggere
 *
 * PATTERN D'USO:
 *   void *p;
 *   do {
 *       p = atomic_load(&shared_ptr);
 *       hp_acquire(0, p);
 *   } while (p != atomic_load(&shared_ptr)); // Verifica non cambiato
 *   // Ora p è sicuro da usare
 *   ... usa p ...
 *   hp_release(0);
 */
void hp_acquire(int slot, void *ptr);

/**
 * hp_release - Rilascia un hazard pointer (non più in uso).
 * @slot: Slot da rilasciare
 */
void hp_release(int slot);

/**
 * hp_release_all - Rilascia tutti gli hazard pointer del thread corrente.
 */
void hp_release_all(void);

/**
 * hp_retire - Segna un puntatore per dealloc futura (quando non più hazardous).
 * @ptr:     Puntatore da deallocare
 * @free_fn: Funzione di dealloc (es. arena_free, free, ecc.)
 *           se NULL, usa free() standard
 */
void hp_retire(void *ptr, void (*free_fn)(void *));

/**
 * hp_scan - Esegue la scansione e dealloca i puntatori sicuri.
 * Chiamato automaticamente da hp_retire quando retire_count > THRESHOLD.
 * Può essere chiamato manualmente anche in momenti di idle.
 */
void hp_scan(void);

/**
 * hp_thread_init - Registra il thread corrente nel subsistema HP.
 * Deve essere chiamato da ogni thread prima di usare HP.
 */
int hp_thread_init(void);

/**
 * hp_thread_exit - Deregistra il thread corrente.
 * Deve essere chiamato prima della terminazione del thread.
 */
void hp_thread_exit(void);

/* ── Macro convenience ──────────────────────────────────────── */

/*
 * HP_LOAD_SAFE — Carica atomicamente un puntatore condiviso
 * proteggendolo con un hazard pointer.
 *
 * Uso:
 *   void *ptr = HP_LOAD_SAFE(0, &shared_atomic_ptr);
 *   // ptr è protetto, usalo liberamente
 *   hp_release(0);
 */
#define HP_LOAD_SAFE(slot, atomic_ptr_addr) ({                                     \
    void *_p;                                                                      \
    do {                                                                           \
        _p = atomic_load_explicit((atomic_ptr_addr), memory_order_acquire);        \
        hp_acquire((slot), _p);                                                    \
    } while (_p != atomic_load_explicit((atomic_ptr_addr), memory_order_acquire)); \
    _p;                                                                            \
})

/*
 * HP_RETIRE_ARENA — Versione specializzata per arena-allocated objects
 * (usa arena_free invece di free standard)
 */
#define HP_RETIRE_ARENA(ptr) hp_retire((ptr), NULL)

#endif /* NEXCACHE_HAZARD_PTR_H */
