/* NexSegcache — Storage TTL-optimized (da Pelikan/CMU NSDI'21)
 * ============================================================
 * Ispirato da Segcache di Twitter/Carnegie Mellon University
 * (NSDI'21 Best Paper Award).
 *
 * Differenza fondamentale:
 *   Memcached (slab): raggruppa oggetti per DIMENSIONE → spreco con TTL variabili
 *   Redis (dict):     overhead 64 bit per entry → +60% memoria sprecata
 *   NexSegcache:      raggruppa oggetti per TTL SIMILE → compaction efficiente
 *
 * Risultati documentati dal paper originale:
 *   - Metadati per oggetto: ~8 bytes vs 56 Memcached vs 64 Redis
 *   - Risparmio memoria: fino al 60% per workload TTL-heavy
 *   - Scalabilità: 8x throughput di Memcached su 24 thread
 *   - Expiration O(1): libera un intero segmento in un colpo
 *
 * Quando NexCache usa NexSegcache invece di NexDashTable:
 *   - avg_object_size < 512 bytes E ttl_prevalence > 70%
 *   - Configurabile manualmente: `storage-engine segcache`
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_SEGCACHE_H
#define NEXCACHE_SEGCACHE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* ── Configurazione ─────────────────────────────────────────── */
#define SEG_DEFAULT_SIZE (1024 * 1024) /* 1MB per segmento */
#define SEG_MAX_SEGMENTS 65536
#define SEG_TTL_BUCKETS 1024  /* Bucket TTL: ogni bucket ≈ intervallo */
#define SEG_TTL_GRANULARITY 1 /* Secondi di granularità per bucket */
#define SEG_HASH_POWER 20     /* 2^20 = 1M bucket nella hash table */
#define SEG_HASH_BUCKETS (1 << SEG_HASH_POWER)
#define SEG_ITEMS_PER_BUCKET 8 /* Bulk-chaining: 8 slot per bucket */

/* ── Item header (8 bytes totali — vs 56 Memcached, 64 Redis) ─ */
/*
 * Layout compatto per oggetti piccoli con TTL:
 *  bit 63-48: key_hash[15:0]    (fingerprint per ridurre false positive)
 *  bit 47-32: key_len[15:0]     (max 65535 byte)
 *  bit 31-16: val_len[15:0]     (max 65535 byte)
 *  bit 15- 0: ttl_bucket[15:0]  (indice bucket TTL)
 */
typedef uint64_t SegItemHeader;

#define SEG_HDR_KEY_HASH(h) (((h) >> 48) & 0xFFFF)
#define SEG_HDR_KEY_LEN(h) (((h) >> 32) & 0xFFFF)
#define SEG_HDR_VAL_LEN(h) (((h) >> 16) & 0xFFFF)
#define SEG_HDR_TTL_BUCKET(h) ((h) & 0xFFFF)

/* ── Segmento: log di item con TTL simile ────────────────────── */
typedef struct NexSegment {
    uint8_t *data;           /* Buffer dati (item packed == header+key+value) */
    uint32_t data_size;      /* Dimensione totale buffer */
    uint32_t write_offset;   /* Offset prossima scrittura */
    uint32_t n_items;        /* Numero oggetti nel segmento */
    uint32_t ttl_bucket;     /* TTL bucket: tutti gli oggetti hanno TTL simile */
    uint32_t create_time;    /* Quando è stato creato */
    uint32_t evict_age;      /* Se molto vecchio, da evict */
    struct NexSegment *next; /* Lista per TTL bucket */
    struct NexSegment *prev;
} NexSegment;

/* ── Hash table entry (bulk-chaining, 8 slot per bucket) ──────── */
typedef struct SegHashEntry {
    uint16_t key_fp;      /* Fingerprint chiave (16 bit) */
    uint16_t seg_idx;     /* Indice del segmento */
    uint32_t item_offset; /* Offset dell'item nel segmento */
} SegHashEntry;

typedef struct SegHashBucket {
    SegHashEntry entries[SEG_ITEMS_PER_BUCKET];
    uint8_t occupancy; /* Bit mask slot occupati */
    uint8_t _pad[3];
} SegHashBucket;

/* ── Statistiche ─────────────────────────────────────────────── */
typedef struct SegStats {
    uint64_t gets;
    uint64_t sets;
    uint64_t dels;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions_segment; /* Interi segmenti evict */
    uint64_t evictions_item;    /* Item singoli evict */
    uint64_t expirations;       /* Item scaduti rimossi */
    uint64_t bytes_stored;
    uint64_t bytes_freed;
    double hit_rate;
    uint32_t active_segments;
} SegStats;

/* ── NexSegcache (struttura principale) ─────────────────────── */
typedef struct NexSegcache {
    /* Pool di segmenti */
    NexSegment *segments; /* Array pre-allocato di segmenti */
    uint32_t n_segments;  /* Totale segmenti allocati */
    uint32_t n_free;      /* Segmenti liberi */
    uint32_t *free_list;  /* Stack di indici liberi */

    /* Lista per TTL bucket: un segmento attivo per bucket */
    NexSegment **ttl_buckets; /* Array SEG_TTL_BUCKETS di testa */

    /* Hash table bulk-chaining */
    SegHashBucket *hash_table;
    uint32_t hash_mask;

    /* Gestione memoria */
    size_t max_memory;
    size_t used_memory;

    /* Thread expiration */
    pthread_t expire_thread;
    pthread_mutex_t lock;
    int running;

    SegStats stats;
} NexSegcache;

/* ── API ────────────────────────────────────────────────────── */
NexSegcache *segcache_create(size_t max_memory, uint32_t segment_size);
void segcache_destroy(NexSegcache *sc);

int segcache_set(NexSegcache *sc, const char *key, uint16_t key_len, const uint8_t *value, uint16_t value_len, uint32_t ttl_seconds);
int segcache_get(NexSegcache *sc, const char *key, uint16_t key_len, uint8_t *value_buf, uint16_t buf_len, uint16_t *value_len_out);
int segcache_del(NexSegcache *sc, const char *key, uint16_t key_len);
int segcache_exists(NexSegcache *sc, const char *key, uint16_t key_len);

/* Espira segmenti TTL-scaduti: O(1) per segmento */
uint32_t segcache_expire_segments(NexSegcache *sc);

SegStats segcache_get_stats(NexSegcache *sc);
void segcache_print_stats(NexSegcache *sc);

#endif /* NEXCACHE_SEGCACHE_H */
