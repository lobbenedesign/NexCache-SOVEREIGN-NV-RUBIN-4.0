/* NexCache GPU Execution Bridge (NVIDIA CAGRA / cuVS)
 * ============================================================
 * Consente l'offloading algoritmico della ricerca vettoriale
 * per miliardi di embeddings off-host, abilitando un VSEARCH ... GPU
 * del tutto trasparente per il client Redis standard.
 *
 * Copyright (c) 2026 NexCache Project
 */

#ifndef NEXCACHE_CAGRA_GPU_H
#define NEXCACHE_CAGRA_GPU_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Trasferisce il contesto dell'indice HNSW corrente
 * nella memoria VRAM (HBM3) accelerata tramite API RAFT/cuVS.
 * @param index_id Identificatore stringa della chiave
 * @param src_data Puntatore alla RAM host pre-pinnata
 * @param num_vectors Dimensione
 */
int nex_gpu_cagra_offload_index(const char *index_id, const float *src_data, size_t num_vectors);

/**
 * @brief Esegue la KNN Search pura sfruttando l'architettura
 * memory-wall di Hopper (H100/H200). Ritorna top-K.
 * @param index_id L'indice remoto
 * @param query Vector di query embedding
 * @param k Numero di risultati (KNN)
 * @param out_ids Buffer id
 * @param out_dists Buffer distanze
 */
int nex_gpu_cagra_knn_search(const char *index_id, const float *query, int k, uint64_t *out_ids, float *out_dists);

#endif /* NEXCACHE_CAGRA_GPU_H */
