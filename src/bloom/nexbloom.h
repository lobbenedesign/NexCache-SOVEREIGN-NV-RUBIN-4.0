/* NexBloom — Blocked Bloom Filter (Performance-Optimized)
 * ============================================================
 * Ispirato al paper VLDB 2019 "Performance-Optimal Filtering".
 * Caratteristiche:
 *   - Cache-aligned (64 bytes per blocco)
 *   - 1 cache miss per lookup (vs 3-8 Bloom standard)
 *   - SIMD-friendly (8 linee per blocco)
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_NEXBLOOM_H
#define NEXCACHE_NEXBLOOM_H

#include <stdint.h>
#include <stddef.h>

/* Ogni blocco è 512 bit (64 byte), esattamente una cache line */
#define NEXBLOOM_BLOCK_BYTES 64
#define NEXBLOOM_BLOCK_UINT64 (NEXBLOOM_BLOCK_BYTES / sizeof(uint64_t))

typedef struct NexBloom {
    uint8_t *data;       /* Array di blocchi */
    uint32_t num_blocks; /* Numero totale di blocchi */
    uint32_t seed;       /* Seed per hashing */
} NexBloom;

/* API */
NexBloom *nexbloom_create(size_t expected_items, double target_fpr);
void nexbloom_destroy(NexBloom *nb);

void nexbloom_add(NexBloom *nb, uint64_t hash);
int nexbloom_check(NexBloom *nb, uint64_t hash);

void nexbloom_reset(NexBloom *nb);
size_t nexbloom_memory_usage(NexBloom *nb);

#endif /* NEXCACHE_NEXBLOOM_H */
