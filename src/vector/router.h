/* NexVector Router — Auto-select the optimal ANN algorithm
 * ============================================================
 * Ispirato a Paper DiskANN, Filtered-DiskANN e CAGRA.
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXVECTOR_ROUTER_H
#define NEXVECTOR_ROUTER_H

#include <stdint.h>
#include <stddef.h>
#include "quantization.h"

typedef enum {
    ALGO_FLAT,     /* Exact search for < 100k vectors */
    ALGO_HNSW,     /* Optimized HNSW for in-memory < 1M vectors */
    ALGO_DISKANN,  /* SSD-based Vamana for 1M-1B vectors */
    ALGO_FILTERED, /* Metadata-filtered queries */
    ALGO_CAGRA     /* GPU-accelerated cuVS (if available) */
} NexVectorAlgo;

typedef enum {
    VEC_PRECISION_FP16,  /* Default high precision */
    VEC_PRECISION_INT8,  /* 4x memory reduction */
    VEC_PRECISION_BINARY /* 16x-32x memory reduction for binary embeddings */
} NexVectorPrecision;

typedef struct NexVectorDecision {
    NexVectorAlgo algo;
    QuantizationType precision;
    float recall_target;
    int use_gpu;
} NexVectorDecision;

NexVectorDecision nex_vector_route(uint64_t count, int dim, int has_filters, const NexVectorCaps *caps);

#endif /* NEXVECTOR_ROUTER_H */
