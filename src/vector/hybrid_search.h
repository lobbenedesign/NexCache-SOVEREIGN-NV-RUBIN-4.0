/* NexCache Hybrid Search — RRF Fusion Engine
 * ============================================================
 * Ispirato a NexCache FT.HYBRID e paper SIGIR 2009.
 * Caratteristiche:
 *   - RRF (Reciprocal Rank Fusion) score fusion
 *   - Supporto per pesi (Weighted RRF)
 *   - Integrazione Full-Text + Vector Similarity
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_HYBRID_H
#define NEXCACHE_HYBRID_H

#include <stdint.h>
#include <stddef.h>

typedef struct HybridResult {
    char *doc_id;
    double score;
} HybridResult;

typedef struct HybridSearch {
    uint32_t k; /* RRF constant, default 60 */
} HybridSearch;

/* Fusion */
int nex_hybrid_rrf(HybridResult *list1, uint32_t len1, HybridResult *list2, uint32_t len2, HybridResult **out_res, uint32_t *out_len);

#endif /* NEXCACHE_HYBRID_H */
