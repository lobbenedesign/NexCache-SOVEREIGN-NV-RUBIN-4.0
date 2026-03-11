/* NexCache Subkey TTL — Generalizzato a TUTTI i tipi di dato
 * ============================================================
 * Ispirato da KeyDB Subkey Expires (solo per Set),
 * NexCache lo estende a: Hash, Set, ZSet, List, JSON path, VectorSet.
 *
 * Nessun competitor ha questa feature su tutti i tipi! (Marzo 2026):
 *  - Valkey 9: solo Hash Field TTL (HEXPIRE)
 *  - KeyDB: solo Set member TTL
 *  - Redis 8.6: nessuno
 *  - Dragonfly: nessuno
 *
 * Comandi NexCache:
 *  HEXPIRE   key seconds FIELDS N f1 f2 ...  → Hash field TTL
 *  SEXPIRE   key seconds MEMBERS N m1 m2 ...  → Set member TTL
 *  ZEXPIRE   key seconds MEMBERS N m1 m2 ...  → ZSet member TTL
 *  LEXPIRE   key seconds INDEX i              → List element TTL
 *  VEXPIRE   key seconds VECTORS N v1 v2 ...  → VectorSet TTL
 *
 * Implementazione:
 *  Ogni struttura dati con subkey TTL porta un SubkeyTTL associato
 *  (puntatore opzionale → zero overhead per strutture senza TTL).
 *  Background thread scansiona SubkeyTTL e rimuove elementi scaduti.
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_SUBKEY_TTL_H
#define NEXCACHE_SUBKEY_TTL_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define SUBKEY_MAX_ENTRIES 65536

/* ── Tipo della struttura padre ──────────────────────────────── */
typedef enum SubkeyContainerType {
    SKC_HASH = 0,
    SKC_SET = 1,
    SKC_ZSET = 2,
    SKC_LIST = 3,
    SKC_JSON = 4,
    SKC_VECTOR = 5,
} SubkeyContainerType;

/* ── Singola entry TTL per un sub-elemento ───────────────────── */
typedef struct SubkeyEntry {
    uint8_t field[256]; /* Nome campo / membro / index serializzato */
    uint8_t field_len;
    uint32_t expire_unix; /* Unix timestamp (secondi) */
} SubkeyEntry;

/* ── SubkeyTTL: mappa campo → expiry per una struttura ─────── */
typedef struct SubkeyTTL {
    SubkeyEntry *entries;
    uint32_t count;
    uint32_t capacity;
    SubkeyContainerType container_type;
    char parent_key[256]; /* Key del container padre */
    pthread_mutex_t lock;
} SubkeyTTL;

/* ── Callback per rimozione elemento scaduto ────────────────── */
typedef void (*SubkeyExpireCb)(const char *parent_key,
                               const uint8_t *field,
                               uint8_t field_len,
                               SubkeyContainerType type,
                               void *ctx);

/* ── API ────────────────────────────────────────────────────── */
SubkeyTTL *subkey_ttl_create(const char *parent_key,
                             SubkeyContainerType type);
void subkey_ttl_destroy(SubkeyTTL *s);

/* Imposta TTL su un campo/membro */
int subkey_ttl_set(SubkeyTTL *s, const uint8_t *field, uint8_t field_len, uint32_t expire_unix);

/* Rimuove TTL da un campo */
int subkey_ttl_clear(SubkeyTTL *s, const uint8_t *field, uint8_t field_len);

/* Ritorna expiry di un campo (0 = no TTL o scaduto) */
uint32_t subkey_ttl_get(SubkeyTTL *s, const uint8_t *field, uint8_t field_len);

/* Scansiona e chiama cb per ogni entry scaduta */
int subkey_ttl_expire(SubkeyTTL *s, SubkeyExpireCb cb, void *ctx);

/* Numero di entry con TTL attivo */
uint32_t subkey_ttl_active_count(SubkeyTTL *s);

/* ── Background scan thread ──────────────────────────────────── */
typedef struct SubkeyTTLManager {
    SubkeyTTL **tables; /* Array di tabelle attive */
    uint32_t count;
    uint32_t capacity;
    pthread_mutex_t lock;
    pthread_t thread;
    int running;
    SubkeyExpireCb global_cb;
    void *global_ctx;
    uint64_t expired_total;
    uint32_t scan_interval_ms; /* Default 1000ms */
} SubkeyTTLManager;

SubkeyTTLManager *subkey_mgr_create(SubkeyExpireCb cb, void *ctx);
void subkey_mgr_destroy(SubkeyTTLManager *mgr);
int subkey_mgr_register(SubkeyTTLManager *mgr, SubkeyTTL *s);
int subkey_mgr_unregister(SubkeyTTLManager *mgr, SubkeyTTL *s);
void subkey_mgr_print_stats(SubkeyTTLManager *mgr);

#endif /* NEXCACHE_SUBKEY_TTL_H */
