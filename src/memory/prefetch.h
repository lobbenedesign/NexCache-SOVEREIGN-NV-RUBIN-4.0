/* NexCache Memory Prefetch Hints — MODULO Memory (v2.0)
 * ============================================================
 * Come Valkey 9.0: usa memory prefetching per ridurre i cache miss
 * quando accediamo a strutture dati predittivamente.
 *
 * Valkey 9.0 ha ottenuto +40% throughput anche grazie a questo.
 * Noi lo implementiamo nativamente con supporto per:
 *   - x86_64 (Intel/AMD): _mm_prefetch, __builtin_prefetch
 *   - ARM64:  prfm (Prefetch Memory instruction)
 *   - Generic: __builtin_prefetch fallback
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_PREFETCH_H
#define NEXCACHE_PREFETCH_H

#include <stddef.h>

/* ── Livelli cache per prefetch ─────────────────────────────── */
typedef enum PrefetchHint {
    PREFETCH_L1 = 0,  /* Porta in L1 cache (più vicina, più piccola) */
    PREFETCH_L2 = 1,  /* Porta in L2 cache */
    PREFETCH_L3 = 2,  /* Porta in L3 cache (più lontana, più grande) */
    PREFETCH_NTA = 3, /* Non-temporal: bypass cache (per large streaming) */
} PrefetchHint;

/* ── Prefetch per lettura ───────────────────────────────────── */
static inline void nex_prefetch_read(const void *addr, PrefetchHint hint) {
    if (!addr) return;
    (void)hint;
#if defined(__x86_64__) || defined(_M_X64)
    switch (hint) {
    case PREFETCH_L1: __builtin_prefetch(addr, 0, 3); break;
    case PREFETCH_L2: __builtin_prefetch(addr, 0, 2); break;
    case PREFETCH_L3: __builtin_prefetch(addr, 0, 1); break;
    case PREFETCH_NTA: __builtin_prefetch(addr, 0, 0); break;
    }
#elif defined(__aarch64__)
    switch (hint) {
    case PREFETCH_L1: __asm__ volatile("prfm pldl1keep, [%0]" ::"r"(addr)); break;
    case PREFETCH_L2: __asm__ volatile("prfm pldl2keep, [%0]" ::"r"(addr)); break;
    case PREFETCH_L3: __asm__ volatile("prfm pldl3keep, [%0]" ::"r"(addr)); break;
    case PREFETCH_NTA: __asm__ volatile("prfm pldl1strm, [%0]" ::"r"(addr)); break;
    }
#else
    __builtin_prefetch(addr, 0, 1);
#endif
}

/* ── Prefetch per scrittura ────────────────────────────────── */
static inline void nex_prefetch_write(void *addr, PrefetchHint hint) {
    if (!addr) return;
    (void)hint;
#if defined(__x86_64__) || defined(_M_X64)
    switch (hint) {
    case PREFETCH_L1: __builtin_prefetch(addr, 1, 3); break;
    case PREFETCH_L2: __builtin_prefetch(addr, 1, 2); break;
    default: __builtin_prefetch(addr, 1, 1); break;
    }
#elif defined(__aarch64__)
    switch (hint) {
    case PREFETCH_L1: __asm__ volatile("prfm pstl1keep, [%0]" ::"r"(addr)); break;
    case PREFETCH_L2: __asm__ volatile("prfm pstl2keep, [%0]" ::"r"(addr)); break;
    default: __asm__ volatile("prfm pstl3keep, [%0]" ::"r"(addr)); break;
    }
#else
    __builtin_prefetch(addr, 1, 1);
#endif
}

/* ── Prefetch array (come Valkey 9 per hash lookup) ─────────── */
/*
 * Tecnica: quando si fa scandita di un array, pre-carichiamo
 * l'elemento N+K mentre processiamo l'elemento N.
 * K = cache_line_size / element_size (tipicamente 8-16 per elementi 64-bit)
 */
#define NEX_PREFETCH_STRIDE 8 /* Pre-carica 8 elementi in avanti */

static inline void nex_prefetch_array(const void **arr, size_t idx, size_t count) {
    size_t ahead = idx + NEX_PREFETCH_STRIDE;
    if (ahead < count) {
        nex_prefetch_read(arr[ahead], PREFETCH_L1);
    }
}

/* ── Macro di convenienza ───────────────────────────────────── */
#define NEX_PREFETCH(ptr) nex_prefetch_read((ptr), PREFETCH_L1)
#define NEX_PREFETCH_L2(ptr) nex_prefetch_read((ptr), PREFETCH_L2)
#define NEX_PREFETCH_WRITE(ptr) nex_prefetch_write((ptr), PREFETCH_L1)

/* FENCE — memory barrier per ARM (garantisce ordine loads/stores) */
#if defined(__aarch64__)
#define NEX_MEMORY_FENCE() __asm__ volatile("dsb ish" ::: "memory")
#else
#define NEX_MEMORY_FENCE() __asm__ volatile("" ::: "memory")
#endif

#endif /* NEXCACHE_PREFETCH_H */
