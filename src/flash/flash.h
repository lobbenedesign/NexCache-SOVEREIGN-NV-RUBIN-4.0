/* NexCache Flash Storage — Header
 * Ispirato a KeyDB FLASH, con AI heat scoring e prefetching predittivo
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_FLASH_H
#define NEXCACHE_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define FLASH_PAGE_SIZE 4096
#define FLASH_MAX_KEY_LEN 512
#define FLASH_MAX_VALUE_LEN (16 * 1024 * 1024) /* 16MB max */
#define FLASH_HEAT_COLD_THRESH 15              /* Sotto questa soglia → sposta su FLASH */
#define FLASH_HEAT_HOT_THRESH 200              /* Sopra questa soglia → preleva da FLASH */
#define FLASH_CO_ACCESS_MAX 1024               /* Max entry nel grafo co-accesso */

/* ── Heat score: 0-255, decresce nel tempo, cresce con accessi ── */
typedef uint8_t HeatScore;

/* ── Voce nel grafo co-accesso (per prefetching predittivo) ──── */
typedef struct CoAccessEdge {
    uint64_t key_hash_a;      /* Key A */
    uint64_t key_hash_b;      /* Key B (sempre dopo A negli accessi) */
    uint32_t co_access_count; /* Quante volte B è stato acceduto dopo A */
    uint32_t total_a_count;   /* Quante volte A è stato acceduto */
    float probability;        /* P(B | A) = co_access / total_a */
} CoAccessEdge;

/* ── Record nel file FLASH ──────────────────────────────────── */
typedef struct FlashRecord {
    uint64_t key_hash;
    uint32_t key_len;
    uint32_t value_len;
    uint32_t expire_unix;
    uint8_t compressed; /* 1 = Zstd, 0 = raw */
    uint8_t _pad[3];
    /* Seguono: key_len bytes + value_len bytes */
} FlashRecord; /* Header fisso 28 bytes */

/* ── Indice in memoria: key_hash → offset nel file FLASH ─────── */
typedef struct FlashIndex {
    uint64_t key_hash;
    uint64_t file_offset; /* Offset nel file FLASH */
    uint32_t record_size; /* Dimensione totale del record */
    HeatScore heat;
    uint8_t _pad[3];
} FlashIndex;

typedef struct FlashStats {
    uint64_t reads;
    uint64_t writes;
    uint64_t evictions;     /* RAM → FLASH */
    uint64_t promotions;    /* FLASH → RAM */
    uint64_t prefetches;    /* Prefetch predittivi eseguiti */
    uint64_t prefetch_hits; /* Prefetch che sono stati utili */
    uint64_t bytes_written;
    uint64_t bytes_read;
    uint64_t cooling_ops; /* Heat score decrements */
    double hit_rate;
} FlashStats;

typedef struct FlashConfig {
    char path[512];               /* Percorso file storage */
    size_t max_size_bytes;        /* Max dimensione file FLASH */
    int compression;              /* 1=Zstd per valori > 4KB */
    int prefetch_enabled;         /* 1=prefetch predittivo */
    uint32_t cooling_interval_ms; /* Ogni quanti ms decrementa heat */
    uint8_t heat_cold_thresh;     /* Soglia per spostare su FLASH */
    uint8_t heat_hot_thresh;      /* Soglia per riaportare in RAM */
} FlashConfig;

typedef struct FlashStorage {
    int fd; /* File descriptor storage file */
    FlashConfig config;

    /* Indice in memoria */
    FlashIndex *index;
    uint32_t index_count;
    uint32_t index_cap;

    /* Grafo co-accesso per prefetching */
    CoAccessEdge *co_graph;
    uint32_t co_count;
    uint32_t co_cap;

    /* Heat cooling thread */
    pthread_t cooling_thread;
    pthread_mutex_t lock;
    int running;

    /* Stats */
    FlashStats stats;
    size_t file_size;
    size_t write_offset; /* Puntatore scrittura (append-only) */
} FlashStorage;

/* ── API ────────────────────────────────────────────────────── */
FlashStorage *flash_create(const FlashConfig *cfg);
void flash_destroy(FlashStorage *fs);

/* Scrivi un valore su FLASH (eviction da RAM) */
int flash_write(FlashStorage *fs, uint64_t key_hash, const char *key, uint32_t key_len, const uint8_t *value, uint32_t value_len, uint32_t expire_unix);

/* Leggi un valore da FLASH (promoting in RAM) */
int flash_read(FlashStorage *fs, uint64_t key_hash, uint8_t *value_buf, uint32_t buf_len, uint32_t *value_len_out);

/* Rimuovi una entry da FLASH */
int flash_delete(FlashStorage *fs, uint64_t key_hash);

/* TTL management */
int flash_expire(FlashStorage *fs, uint64_t key_hash, uint32_t expire_unix);
int64_t flash_ttl(FlashStorage *fs, uint64_t key_hash);

/* Svuota il database */
void flash_flush(FlashStorage *fs);

/* Aggiorna heat score (chiamato ad ogni accesso) */
void flash_record_access(FlashStorage *fs, uint64_t key_hash_a, uint64_t key_hash_b_after);

/* Controlla se una chiave deve essere promossa in RAM */
int flash_should_promote(FlashStorage *fs, uint64_t key_hash);

/* Esegue prefetch predittivo: carica in RAM le chiavi probabili */
int flash_prefetch(FlashStorage *fs, uint64_t trigger_key_hash, void (*load_to_ram_cb)(uint64_t key_hash, const uint8_t *value, uint32_t vlen, void *ctx), void *ctx);

FlashStats flash_get_stats(FlashStorage *fs);
void flash_print_stats(FlashStorage *fs);

#endif /* NEXCACHE_FLASH_H */
