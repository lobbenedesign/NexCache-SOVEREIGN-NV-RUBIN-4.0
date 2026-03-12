/* NexCache Persistence — Header
 * AOF (Append Only File) + RDB (point-in-time snapshots)
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_PERSISTENCE_H
#define NEXCACHE_PERSISTENCE_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <pthread.h>

/* ── Modalità AOF ───────────────────────────────────────────── */
typedef enum AOFMode {
    AOF_DISABLED = 0,
    AOF_ALWAYS = 1,   /* fsync dopo ogni write: durabilità max */
    AOF_EVERYSEC = 2, /* fsync ogni secondo: bilanciamento */
    AOF_NO = 3,       /* OS decide: performance max */
} AOFMode;

/* ── Tipo entry AOF ─────────────────────────────────────────── */
typedef enum AOFEntryType {
    AOF_OP_SET = 1,
    AOF_OP_DEL = 2,
    AOF_OP_EXPIRE = 3,
    AOF_OP_FLUSH = 4,
    AOF_OP_SELECT = 5,
} AOFEntryType;

/* ── Header AOF entry (16 bytes, packed) ────────────────────── */
typedef struct __attribute__((packed)) AOFEntry {
    uint8_t type;       /* AOFEntryType */
    uint8_t flags;      /* bit0=compressed */
    uint16_t key_len;   /* Lunghezza chiave */
    uint32_t value_len; /* Lunghezza valore (0 per DEL) */
    uint64_t expire_us; /* Expiry timestamp µs (0 = no expiry) */
    /* Seguito da key_len bytes chiave + value_len bytes valore */
} AOFEntry;

#define AOF_MAGIC "NEXAOF1"
#define AOF_MAGIC_LEN 7
#define RDB_MAGIC "NEXRDB1"
#define RDB_MAGIC_LEN 7

/* ── Statistiche persistence ─────────────────────────────────── */
typedef struct PersistStats {
    uint64_t aof_writes;        /* Numero di write su AOF */
    uint64_t aof_bytes;         /* Byte scritti su AOF */
    uint64_t aof_fsyncs;        /* fsync eseguiti */
    uint64_t rdb_saves;         /* RDB snapshot salvati */
    uint64_t rdb_bytes;         /* Byte nell'ultimo RDB */
    double last_rdb_save_sec;   /* Quanto tempo ha impiegato */
    uint64_t aof_rewrite_count; /* Numero riscritture AOF */
    uint64_t recovery_ops;      /* Operazioni ripristinate da AOF/RDB */
} PersistStats;

/* ── Configurazione ──────────────────────────────────────────── */
typedef struct PersistConfig {
    AOFMode aof_mode;             /* Modalità AOF */
    char aof_path[512];           /* Path del file AOF */
    char rdb_path[512];           /* Path del file RDB */
    int rdb_auto_save;            /* 1 = auto-save RDB abilitato */
    uint32_t rdb_save_interval_s; /* Ogni quanti secondi salvare RDB */
    uint32_t rdb_max_dirty_keys;  /* Salva se >N chiavi changed */
    int aof_rewrite_enabled;      /* 1 = riscrivi AOF quando cresce troppo */
    uint64_t aof_rewrite_size_mb; /* Riscrivi se AOF > N MB */
} PersistConfig;

/* ── API AOF ─────────────────────────────────────────────────── */
int aof_init(const PersistConfig *cfg);
void aof_shutdown(void);
int aof_write(AOFEntryType type, const char *key, size_t key_len, const uint8_t *value, size_t value_len, uint64_t expire_us);
int aof_fsync_now(void);
int aof_rewrite(void);
int aof_replay(const char *path,
               int (*apply_fn)(AOFEntryType, const char *, size_t, const uint8_t *, size_t, uint64_t));

/* ── API RDB ─────────────────────────────────────────────────── */
int rdb_save(const char *path,
             int (*iter_fn)(void *, char *, size_t, uint8_t **, size_t *, uint64_t *),
             void *iter_ctx);
int rdb_load(const char *path,
             int (*apply_fn)(const char *, size_t, const uint8_t *, size_t, uint64_t));

/* Stats */
PersistStats persist_get_stats(void);
void persist_print_stats(void);

#endif /* NEXCACHE_PERSISTENCE_H */
