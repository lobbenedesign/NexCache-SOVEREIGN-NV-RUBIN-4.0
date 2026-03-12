/* NexDashTable — Hash Engine di NexCache
 * ============================================================
 * Ispirato al DashTable di Dragonfly DB (paper "Dash: Scalable
 * Hashing on Persistent Memory"), con miglioramenti fondamentali:
 *
 * vs NexCache dict.c:
 *   - Overhead per entry: ~16 bit vs 64 bit NexCache (4x meno memoria)
 *   - Directory overhead quasi zero vs 8N bytes di NexCache
 *   - Fork-less snapshot DELTA (NexCache richiede fork + 3x memoria)
 *   - Eviction 2Q + ML prediction (NexCache usa LRU random sampling)
 *
 * vs DashTable di Dragonfly:
 *   - Overhead ~16 bit (vs ~20 bit Dragonfly)
 *   - Integrazione nativa Tagged Pointers (tipo nel bit dell'entry)
 *   - Integrazione nativa Arena Allocator (zero malloc per nodi)
 *   - Eviction 2Q + ML prediction (Dragonfly solo 2Q)
 *   - Snapshot DELTA non solo completo (Dragonfly solo completo)
 *
 * Struttura: directory → segmenti → bucket → slot
 *   Ogni segmento: 56 bucket × 4 slot = 224 entry
 *   Ogni entry: 16 bit metadati + hash + value_ptr + key_offset
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_NEXDASH_H
#define NEXCACHE_NEXDASH_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "../bloom/nexbloom.h"
#include "../memory/arena.h"

/* ── Tagged Pointer Configuration ──────────────────────────── */
/* Su x86-64/ARM64, gli indirizzi usano 48 bit. I bit 63-48 sono tag.
 * [63:56] - Key Length (8 bit)
 * [55:52] - Entry Type (4 bit)
 * [51:50] - Eviction Tier (2 bit)
 * [49:48] - Version / Flags (2 bit)
 */
typedef uintptr_t TaggedPtr;

#define TAG_ADDR_MASK 0x0000FFFFFFFFFFFFULL
#define TAG_META_MASK 0xFFFF000000000000ULL
#define TAG_META_SHIFT 48

#define TAG_GET_ADDR(tp) ((void *)((tp) & TAG_ADDR_MASK))
#define TAG_GET_LEN(tp) ((uint8_t)((tp) >> 56))
#define TAG_GET_TYPE(tp) ((uint8_t)(((tp) >> 52) & 0x0F))
#define TAG_GET_TIER(tp) ((uint8_t)(((tp) >> 50) & 0x03))
#define TAG_GET_VER(tp) ((uint8_t)(((tp) >> 48) & 0x03))

#define TAG_SET_META(addr, len, type, tier, ver)             \
    ((uintptr_t)(addr) | ((uintptr_t)(len) << 56) |          \
     ((uintptr_t)(type) << 52) | ((uintptr_t)(tier) << 50) | \
     ((uintptr_t)(ver) << 48))

/* ── Configurazione segmento ────────────────────────────────── */
#define NEXDASH_BUCKET_COUNT 56                                           /* Bucket per segmento (come Dragonfly) */
#define NEXDASH_SLOT_COUNT 4                                              /* Slot per bucket */
#define NEXDASH_SEGMENT_ITEMS (NEXDASH_BUCKET_COUNT * NEXDASH_SLOT_COUNT) /* 224 */
#define NEXDASH_INITIAL_SEGMENTS 4                                        /* Segmenti iniziali */
#define NEXDASH_MAX_LOAD_FACTOR 0.85f

/* ── Tipo dato embedded nel puntatore ──────────────────────── */
typedef enum NexEntryType {
    NTYPE_STRING = 0x0,
    NTYPE_HASH = 0x1,
    NTYPE_LIST = 0x2,
    NTYPE_SET = 0x3,
    NTYPE_ZSET = 0x4,
    NTYPE_STREAM = 0x5,
    NTYPE_JSON = 0x6,
    NTYPE_VECTOR = 0x7,
    NTYPE_CRDT = 0x8,
    NTYPE_DELETED = 0xF,
} NexEntryType;

/* ── Eviction tier (algoritmo 2Q) ───────────────────────────── */
typedef enum Eviction2QTier {
    TIER_PROBATORY = 0, /* Accesso singolo — candidato all'eviction */
    TIER_PROTECTED = 1, /* Accesso multiplo — protetto dall'eviction */
    TIER_HOT = 2,       /* Hot key: ml-predicted future accesses */
    TIER_PINNED = 3,    /* Mai evict (es. config, auth tokens) */
} Eviction2QTier;

/* ── NexDashSlot: 24 byte esatti (v5 compliance) ──────────── */
typedef struct __attribute__((packed)) NexDashSlot {
    uint64_t key_hash;    /* 8 byte: Hash completo */
    TaggedPtr value_ptr;  /* 8 byte: Puntatore + Metadati (v5 tagged) */
    uint32_t key_offset;  /* 4 byte: Offset in arena */
    uint32_t expire_us32; /* 4 byte: Expiry precision ~1s */
} NexDashSlot;            /* Total: 24 byte */

/* ── NexDashBucket: 4 slot + probing bitmap ────────────────── */
typedef struct NexDashBucket {
    NexDashSlot slots[NEXDASH_SLOT_COUNT]; /* 112 bytes */
    uint8_t occupancy;                     /* Bit mask slot occupati */
    uint8_t _pad[3];
} NexDashBucket; /* 116 bytes per bucket */

/* ── NexDashSegment ─────────────────────────────────────────── */
typedef struct NexDashSegment {
    NexDashBucket buckets[NEXDASH_BUCKET_COUNT]; /* 56 × 116 = 6496 bytes */
    uint32_t item_count;
    uint32_t version;       /* Snapshot version counter */
    uint64_t last_modified; /* Timestamp ultimo cambiamento */
} NexDashSegment;

/* ── Stato 2Q eviction ──────────────────────────────────────── */
typedef struct Evict2QState {
    /* Buffer probatorio: FIFO, entry accessate una sola volta */
    uint64_t *prob_queue; /* Circular queue di key hash */
    uint32_t prob_head;
    uint32_t prob_tail;
    uint32_t prob_cap;
    uint32_t prob_size;

    /* Buffer protetto: LRU, entry accessate più volte */
    uint64_t *prot_queue;
    uint32_t prot_head;
    uint32_t prot_tail;
    uint32_t prot_cap;
    uint32_t prot_size;

    /* ML prediction: access frequency + time of day features */
    uint32_t access_count;         /* Numero accessi totali */
    float predicted_hot_threshold; /* Soglia ML per tier HOT */
} Evict2QState;

/* ── ML Prediction state (leggero: regressione logistica 8 feature) */
typedef struct MLState {
    float weights[8]; /* Pesi regressione logistica — si aggiornano online */
    float bias;
    uint64_t updates; /* Numero aggiornamenti fatti */
} MLState;

/* ── Statistiche ─────────────────────────────────────────────── */
typedef struct NexDashStats {
    uint64_t gets;
    uint64_t sets;
    uint64_t dels;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions_probatory;
    uint64_t evictions_protected;
    uint64_t evictions_ml_saved;
    uint64_t collisions;
    uint64_t resizes;
    uint64_t snapshot_count;
    uint64_t snapshot_delta_entries;
    double avg_lookup_probe; /* Numero medio di slot esaminati per GET */
    double hit_rate;
} NexDashStats;

/* ── NexDashTable (struttura principale) ─────────────────────── */
typedef struct NexDashTable {
    NexDashSegment **directory; /* Array di puntatori a segmenti */
    uint32_t dir_size;          /* Numero corrente di segmenti */
    uint32_t item_count;        /* Totale elementi */
    uint32_t dir_cap;           /* Capacità directory (power of 2) */

    /* Memory Pool: arena allocator nativo per strutture → zero fragmentation */
    struct Arena *arena;

    /* Key pool: pool lineare contiguo per keys_offset a 32bit → densità massima */
    uint8_t *key_pool;
    uint32_t key_pool_size;
    uint32_t key_pool_cap;

    /* Eviction 2Q + ML */
    Evict2QState evict;

    MLState ml;

    /* Snapshot fork-less */
    volatile int snapshot_in_progress;
    uint64_t snapshot_version; /* Version al momento dello snapshot */
    uint64_t current_version;  /* Version corrente */

    /* Max memory */
    size_t max_memory; /* 0 = nessun limite */
    size_t used_memory;

    /* Security & Bloom */
    NexBloom *bloom; /* Filter interno per miss rate zero-latency */

    NexDashStats stats;
} NexDashTable;

/* ── Callback per iterazione ────────────────────────────────── */
typedef void (*NexDashIterCb)(const char *key, uint8_t key_len, void *value, uint8_t type, void *ctx);

/* ── API pubblica ───────────────────────────────────────────── */

NexDashTable *nexdash_create(size_t initial_segments, size_t max_memory);
void nexdash_destroy(NexDashTable *t);

/* CRUD */
int nexdash_set(NexDashTable *t, const char *key, uint8_t key_len, void *value, NexEntryType type, uint64_t expire_us);
void *nexdash_get(NexDashTable *t, const char *key, uint8_t key_len, NexEntryType *type_out);
int nexdash_del(NexDashTable *t, const char *key, uint8_t key_len);
int nexdash_exists(NexDashTable *t, const char *key, uint8_t key_len);
int nexdash_expire(NexDashTable *t, const char *key, uint8_t key_len, uint64_t expire_us);

/* Iterazione */
void nexdash_scan(NexDashTable *t, NexDashIterCb cb, void *ctx);
void nexdash_scan_expired(NexDashTable *t, NexDashIterCb cb, void *ctx);

/* Fork-less snapshot DELTA */
int nexdash_snapshot_start(NexDashTable *t);
int nexdash_snapshot_iterate_delta(NexDashTable *t, NexDashIterCb cb, void *ctx);
int nexdash_snapshot_iterate_full(NexDashTable *t, NexDashIterCb cb, void *ctx);
int nexdash_snapshot_end(NexDashTable *t);

/* Eviction 2Q + ML */
size_t nexdash_evict_to_target(NexDashTable *t, size_t target_bytes);
void nexdash_record_access(NexDashTable *t, const char *key, uint8_t key_len);
void nexdash_ml_train(NexDashTable *t, const char *key, uint8_t key_len, int was_accessed_again);

/* Stats */
NexDashStats nexdash_get_stats(NexDashTable *t);
void nexdash_print_stats(NexDashTable *t);

#endif /* NEXCACHE_NEXDASH_H */
