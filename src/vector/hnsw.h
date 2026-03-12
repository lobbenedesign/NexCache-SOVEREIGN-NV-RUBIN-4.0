/* NexCache HNSW Vector Index — Header
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_HNSW_H
#define NEXCACHE_HNSW_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* ── Costanti ──────────────────────────────────────────────── */
#define HNSW_DEFAULT_M 16                /* Connessioni per nodo (param M) */
#define HNSW_DEFAULT_EF_CONSTRUCTION 200 /* Beam width build */
#define HNSW_DEFAULT_EF 200              /* Beam width query */
#define HNSW_MAX_LAYERS 16               /* Layer massimi */
#define HNSW_INITIAL_CAPACITY 1024       /* Vettori iniziali */
#define HNSW_INVALID_ID UINT32_MAX

typedef uint32_t hnsw_id_t;

/* ── Metrica distanza ───────────────────────────────────────── */
typedef enum {
    HNSW_METRIC_COSINE = 0,
    HNSW_METRIC_L2 = 1,
    HNSW_METRIC_IP = 2, /* Inner product */
} HNSWMetric;

/* ── Quantizzazione ─────────────────────────────────────────── */
typedef enum {
    HNSW_QUANT_FLOAT32 = 0, /* 4 bytes/dim — precisione massima */
    HNSW_QUANT_INT8 = 1,    /* 1 byte/dim  — 75% risparmio RAM */
    HNSW_QUANT_BINARY = 2,  /* 1 bit/dim   — 97% risparmio RAM */
} QuantType;

/* ── Statistiche ────────────────────────────────────────────── */
typedef struct HNSWStats {
    uint64_t inserts;
    uint64_t searches;
    uint64_t deletes;
    uint64_t distance_calcs;
    double avg_search_us;
    size_t count;
    int max_layer;
    uint64_t memory_bytes;
} HNSWStats;

/* ── Nodo nel grafo ─────────────────────────────────────────── */
typedef struct HNSWNode {
    float *vector;                         /* Vettore raw float32 */
    hnsw_id_t *neighbors[HNSW_MAX_LAYERS]; /* Lista vicini per layer */
    int num_neighbors[HNSW_MAX_LAYERS];
    hnsw_id_t ext_id;   /* ID esterno (chiave NexCache) */
    int level;          /* Layer massimo di questo nodo */
    uint8_t is_deleted; /* Soft delete */
    char metadata[128]; /* Metadata opzionale */
} HNSWNode;

/* ── Indice HNSW ────────────────────────────────────────────── */
typedef struct HNSWIndex {
    HNSWNode *nodes;
    size_t count;
    size_t cap;
    hnsw_id_t enter_point; /* Entry point per tutte le query */
    int max_layer;         /* Layer massimo corrente */
    int dim;               /* Dimensionalità vettori */
    int M;                 /* Max connessioni per nodo (tranne layer 0) */
    int M_max0;            /* Max connessioni layer 0 (= M*2) */
    int ef_construction;
    int ef;      /* ef per le query */
    double mult; /* Moltiplicatore for level generation */
    HNSWMetric metric;
    QuantType quant;
    HNSWStats stats;
    pthread_rwlock_t rwlock;
} HNSWIndex;

/* ── Risultato di ricerca ───────────────────────────────────── */
typedef struct HNSWResult {
    hnsw_id_t id;     /* ID interno */
    hnsw_id_t ext_id; /* ID esterno */
    float distance;   /* Distanza dal query */
    float score;      /* Similarity score (1 - distance per cosine) */
} HNSWResult;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * hnsw_create - Crea un nuovo indice HNSW.
 * @dim: Dimensionalità dei vettori
 * @M:   Parametro di connettività (default 16)
 * @ef_construction: Beam width durante la costruzione (default 200)
 * @metric: Funzione di distanza
 * @quant: Tipo di quantizzazione
 */
HNSWIndex *hnsw_create(int dim, int M, int ef_construction, HNSWMetric metric, QuantType quant);

/**
 * hnsw_add - Inserisce un vettore nell'indice.
 * @ext_id: ID esterno (es. hash della chiave)
 * @vector: Vettore float32 di lunghezza dim
 * @metadata: Metadati opzionali (NULL = nessuno)
 * Returns: 0 su successo, -1 su errore.
 */
int hnsw_add(HNSWIndex *idx, hnsw_id_t ext_id, const float *vector, const char *metadata);

/**
 * hnsw_search - Ricerca i k vettori più simili.
 * @query_vec: Vettore query float32 di lunghezza dim
 * @k: Numero di risultati desiderati
 * @ef: Beam width (0 = usa default)
 * @results: Array output di almeno k elementi
 * @num_results: Numero di risultati effettivamente trovati
 * Returns: 0 su successo.
 */
int hnsw_search(HNSWIndex *idx,
                const float *query_vec,
                int k,
                int ef,
                HNSWResult *results,
                int *num_results);

/**
 * hnsw_delete - Soft-deletes un vettore (non ricostruisce il grafo).
 */
int hnsw_delete(HNSWIndex *idx, hnsw_id_t ext_id);

/**
 * hnsw_get_stats - Statistiche dell'indice.
 */
HNSWStats hnsw_get_stats(HNSWIndex *idx);

/**
 * hnsw_print_stats - Stampa statistiche human-readable.
 */
void hnsw_print_stats(HNSWIndex *idx);

/**
 * hnsw_destroy - Dealloca l'indice.
 */
void hnsw_destroy(HNSWIndex *idx);

#endif /* NEXCACHE_HNSW_H */
