/* NexCache Arena Allocator — MODULO 1
 * ============================================================
 * Sostituisce zmalloc() con un allocatore ad arena per eliminare
 * frammentazione della memoria e cache miss della CPU.
 *
 * Vantaggi vs malloc():
 *   - Allocazione O(1) garantita (bump pointer)
 *   - Zero frammentazione — tutti gli oggetti correlati contigui
 *   - Reset O(1) senza free individuale (riuso memoria)
 *   - Cache locality: oggetti allocati insieme stanno vicini in memoria
 *   - Thread-local arenas per worker: zero lock sulla hot path
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_ARENA_H
#define NEXCACHE_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/mman.h>

/* ── Costanti dimensione arena ─────────────────────────────── */
#define ARENA_SMALL_SIZE    (1UL  * 1024 * 1024)   /* 1MB  — oggetti piccoli */
#define ARENA_MEDIUM_SIZE   (16UL * 1024 * 1024)   /* 16MB — oggetti medi   */
#define ARENA_LARGE_SIZE    (256UL* 1024 * 1024)   /* 256MB — oggetti grandi*/
#define ARENA_DEFAULT_SIZE  ARENA_MEDIUM_SIZE

/* Allineamento cache line CPU (x86-64/ARM: 64 byte) */
#define ARENA_CACHE_LINE    64
#define ARENA_ALIGNMENT     ARENA_CACHE_LINE

/* Quante volte il blocco cresce se pieno */
#define ARENA_GROWTH_FACTOR 2

/* Tag magico per rilevare corruzione memoria */
#define ARENA_MAGIC         0xA12E4A30A12E4A30ULL

/* ── Struttura blocco interno ───────────────────────────────── */
typedef struct ArenaBlock {
    uint64_t  magic;           /* Protezione corruzione */
    uint8_t  *base;            /* Indirizzo base del blocco (mmap) */
    size_t    size;            /* Dimensione totale allocata */
    size_t    used;            /* Bytes già usati (bump pointer) */
    size_t    peak;            /* Peak usage (statistiche) */
    struct ArenaBlock *next;   /* Lista linked per blocchi multipli */
    struct ArenaBlock *prev;
} ArenaBlock;

/* ── Statistiche arena ──────────────────────────────────────── */
typedef struct ArenaStats {
    uint64_t  total_allocs;    /* Numero totale allocazioni */
    uint64_t  total_bytes;     /* Bytes totali allocati */
    uint64_t  wasted_bytes;    /* Bytes sprecati (padding allineamento) */
    uint64_t  block_count;     /* Numero blocchi allocati */
    uint64_t  reset_count;     /* Numero reset eseguiti */
    double    avg_alloc_ns;    /* Latenza media allocazione (nanosec) */
} ArenaStats;

/* ── Struttura arena principale ─────────────────────────────── */
typedef struct Arena {
    ArenaBlock   *current;         /* Blocco corrente per allocazione */
    ArenaBlock   *head;            /* Primo blocco (per reset/destroy) */
    size_t        default_block_sz;/* Dimensione default nuovi blocchi */
    ArenaStats    stats;           /* Statistiche performance */
    pthread_mutex_t lock;          /* Lock (solo per arena condivise) */
    int           is_thread_local; /* 1 = nessun lock necessario */
    char          name[64];        /* Nome descrittivo (debug) */
} Arena;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * arena_create - Crea una nuova arena.
 * @block_size: Dimensione del primo blocco (usa ARENA_DEFAULT_SIZE se 0)
 * @name:       Nome descrittivo per il debug
 * @thread_local: 1 se usata da un solo thread (disabilita lock)
 *
 * Usa mmap() invece di malloc() per controllo completo della memoria.
 * Returns: puntatore all'arena, NULL su errore.
 */
Arena *arena_create(size_t block_size, const char *name, int thread_local_mode);

/**
 * arena_alloc - Alloca `size` bytes nell'arena.
 * Thread-safe (a meno che is_thread_local = 1).
 * Returns: puntatore alla memoria, NULL su errore.
 */
void *arena_alloc(Arena *arena, size_t size);

/**
 * arena_alloc_aligned - Alloca `size` bytes con allineamento specifico.
 * Necessario per operazioni SIMD (AVX-512 richiede 64 byte).
 */
void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

/**
 * arena_alloc_zeroed - Come arena_alloc ma azzera la memoria.
 */
void *arena_alloc_zeroed(Arena *arena, size_t size);

/**
 * arena_strdup - Duplica una stringa nell'arena.
 */
char *arena_strdup(Arena *arena, const char *str);

/**
 * arena_strndup - Duplica al massimo n caratteri di una stringa.
 */
char *arena_strndup(Arena *arena, const char *str, size_t n);

/**
 * arena_reset - Azzera il cursore di allocazione senza liberare memoria.
 * Estremamente veloce (O(1)) — riusa lo stesso spazio fisico.
 * ATTENZIONE: tutti i puntatori precedenti diventano invalidi.
 */
void arena_reset(Arena *arena);

/**
 * arena_destroy - Libera tutta la memoria dell'arena con munmap().
 */
void arena_destroy(Arena *arena);

/**
 * arena_used - Bytes correntemente usati nell'arena.
 */
size_t arena_used(Arena *arena);

/**
 * arena_capacity - Bytes totali allocati (inclusi quelli non usati).
 */
size_t arena_capacity(Arena *arena);

/**
 * arena_wasted - Bytes sprecati per allineamento.
 */
size_t arena_wasted(Arena *arena);

/**
 * arena_stats - Restituisce una copia delle statistiche.
 */
ArenaStats arena_get_stats(Arena *arena);

/**
 * arena_print_stats - Stampa statistiche human-readable su stderr.
 */
void arena_print_stats(Arena *arena);

/* ── Macro type-safe ────────────────────────────────────────── */

/** Alloca un singolo oggetto del tipo dato, allineato correttamente. */
#define ARENA_NEW(arena, T) \
    ((T*)arena_alloc_aligned((arena), sizeof(T), _Alignof(T)))

/** Alloca un array di N oggetti del tipo dato. */
#define ARENA_NEW_ARRAY(arena, T, N) \
    ((T*)arena_alloc_aligned((arena), sizeof(T) * (size_t)(N), _Alignof(T)))

/** Alloca e azzera un singolo oggetto. */
#define ARENA_NEW_ZERO(arena, T) \
    ((T*)arena_alloc_zeroed((arena), sizeof(T)))

/** Alloca e azzera un array. */
#define ARENA_NEW_ARRAY_ZERO(arena, T, N) \
    ((T*)(__extension__({ \
        void *_p = arena_alloc_aligned((arena), sizeof(T)*(N), _Alignof(T)); \
        if (_p) __builtin_memset(_p, 0, sizeof(T)*(N)); \
        _p; \
    })))

/* ── Arena globale per bootstrapping ───────────────────────── */
extern Arena *g_nexcache_arena;  /* Arena globale condivisa */

/**
 * nexcache_arena_init - Inizializza l'arena globale. Da chiamare all'avvio.
 */
int nexcache_arena_init(void);

/**
 * nexcache_arena_shutdown - Libera l'arena globale. Da chiamare allo shutdown.
 */
void nexcache_arena_shutdown(void);

#endif /* NEXCACHE_ARENA_H */
