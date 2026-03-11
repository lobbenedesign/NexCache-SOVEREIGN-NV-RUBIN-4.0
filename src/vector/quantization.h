/* NexCache Vector Quantization Module
 * ============================================================
 * Supporto per quantizzazione nativa ad alte prestazioni per vettori.
 * Il modulo include auto-rilevamento hardware (AVX-512, NEON, SVE)
 * e selezione dinamica del miglior algoritmo.
 *
 * Copyright (c) 2026 NexCache Project
 */

#ifndef NEXCACHE_VECTOR_QUANTIZATION_H
#define NEXCACHE_VECTOR_QUANTIZATION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    QUANT_FP32 = 0,   // nessuna quantizzazione
    QUANT_FP16 = 1,   // -50% RAM, quasi zero degradazione
    QUANT_INT8 = 2,   // -75% RAM, AVX-512 VNNI, 3-4x speedup
    QUANT_BINARY = 3, // -97% RAM, Hamming, 10-20x speedup
} QuantizationType;

typedef struct NexVectorCaps {
    bool avx512f;       // AVX-512 Foundation
    bool avx512vnni;    // AVX-512 VNNI — necessario per INT8 optimal
    bool avx512bf16;    // BF16 acceleration (Intel Cooper Lake+)
    bool arm_neon;      // ARM NEON — FP16 nativo su ARM
    bool arm_sve;       // ARM SVE — scalable vector extension
    bool gpu_available; // CUDA/ROCm per CAGRA
    int gpu_vram_mb;    // VRAM disponibile in MB
} NexVectorCaps;

// Rileva le capabilities SIMD della CPU
void nexvec_probe_caps(NexVectorCaps *caps);

// Auto-seleziona la migliore quantizzazione disponibile
QuantizationType nexvec_auto_quantization(const NexVectorCaps *caps,
                                          size_t n_vectors,
                                          float recall_threshold);

void nex_vector_quantize_int8(const float *src, int8_t *dst, size_t dim);
void nex_vector_quantize_binary(const float *src, uint8_t *dst, size_t dim);

int32_t nex_vector_dot_int8(const int8_t *a, const int8_t *b, size_t dim);
uint32_t nex_vector_hamming_dist(const uint8_t *a, const uint8_t *b, size_t num_bytes);

#endif /* NEXCACHE_VECTOR_QUANTIZATION_H */
