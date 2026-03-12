/* NexCache Multi-Tenancy & Quota Enforcement — MODULO 12 (v2.0)
 * ============================================================
 * Fondamentale per SaaS — nessuno dei due competitor lo ha.
 *
 * Il problema con NexCache/NexCache multi-tenant:
 *   - Un tenant che esegue KEYS * blocca TUTTI gli altri tenant
 *   - Un tenant che riempie la RAM causa eviction per TUTTI
 *   - Non c'è modo di garantire SLA per tenant singolo
 *
 * La soluzione NexCache:
 *   Namespace isolati con risorse GARANTITE e LIMITATE per tenant.
 *   Isolamento a livello di CPU, memoria, connessioni, bandwidth.
 *   ACL e metriche per tenant separati.
 *
 * Comandi NexCache:
 *   NSCONFIG <ns> SET memory-limit 1GB
 *   NSCONFIG <ns> SET cpu-budget 25%
 *   NSCONFIG <ns> SET bandwidth 100MB/s
 *   NSCONFIG <ns> SET max-keys 10000000
 *   NSCONFIG <ns> SET max-connections 1000
 *   NSINFO   <ns>        → stats complete del namespace
 *   NSLIST              → lista tutti i namespace attivi
 *   NSEVICT  <ns> <mb>  → forza eviction per namespace
 *   NSDROP   <ns>       → elimina namespace e tutti i suoi dati
 *
 * Copyright (c) 2026 NexCache Project — BSD License (enterprise)
 */

#ifndef NEXCACHE_QUOTA_H
#define NEXCACHE_QUOTA_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* ── Costanti ───────────────────────────────────────────────── */
#define QUOTA_MAX_NAMESPACES 256
#define QUOTA_NS_NAME_MAX 64
#define QUOTA_TOKEN_SIZE 64
#define QUOTA_DEFAULT_MAX_KEYS UINT64_MAX
#define QUOTA_WINDOW_SECS 1 /* Finestra di misurazione rate */

/* ── Politica di enforcement quando il limite è raggiunto ───── */
typedef enum QuotaAction {
    QUOTA_ACTION_REJECT = 0,   /* Rifiuta il comando con errore */
    QUOTA_ACTION_THROTTLE = 1, /* Rallenta la risposta (rate limiting) */
    QUOTA_ACTION_EVICT = 2,    /* Forza eviction delle chiavi più fredde */
    QUOTA_ACTION_LOG_ONLY = 3, /* Logga ma non blocca (soft limit) */
} QuotaAction;

/* ── Configurazione di un namespace ────────────────────────── */
typedef struct NamespaceConfig {
    char name[QUOTA_NS_NAME_MAX];

    /* Limiti memoria */
    uint64_t max_memory_bytes; /* 0 = illimitato */
    uint64_t max_keys;         /* 0 = illimitato */
    QuotaAction memory_action; /* Azione a limite raggiunto */

    /* Limiti CPU (percentuale del totale) */
    double cpu_budget_pct;    /* 0.0-100.0, 0 = illimitato */
    uint64_t max_ops_per_sec; /* Operazioni/sec max */
    QuotaAction cpu_action;

    /* Limiti rete */
    uint64_t max_bandwidth_bps; /* Bytes/sec max in+out, 0 = illimitato */
    uint32_t max_connections;   /* Connessioni simultanee max */
    QuotaAction network_action;

    /* TTL automatico per tutte le chiavi del namespace */
    int default_ttl_secs; /* 0 = nessun TTL default */

    /* Eviction policy specifica per namespace */
    int eviction_policy; /* 0=noeviction, 1=lru, 2=lfu, 3=lrm */

    /* Isolamento degli slot hash */
    int dedicated_workers; /* 1 = worker dedicati per questo ns */
} NamespaceConfig;

/* ── Statistiche real-time di un namespace ──────────────────── */
typedef struct NamespaceStats {
    char name[QUOTA_NS_NAME_MAX];

    /* Uso corrente risorse */
    uint64_t used_memory_bytes;
    uint64_t peak_memory_bytes;
    uint64_t key_count;
    uint32_t active_connections;

    /* Throughput corrente */
    uint64_t ops_total;
    double ops_per_sec;
    uint64_t bytes_read;
    uint64_t bytes_written;
    double bandwidth_bps;

    /* Latenza */
    double latency_p50_us;
    double latency_p99_us;
    double latency_max_us;

    /* Hit rate */
    uint64_t hits;
    uint64_t misses;
    double hit_rate;

    /* Violazioni quota */
    uint64_t quota_violations_memory;
    uint64_t quota_violations_cpu;
    uint64_t quota_violations_network;
    uint64_t requests_rejected;
    uint64_t requests_throttled;
    uint64_t evictions_forced;

    /* Timestamp */
    uint64_t created_at_us;
    uint64_t last_access_us;
} NamespaceStats;

/* ── Struttura namespace ─────────────────────────────────────── */
typedef struct Namespace {
    NamespaceConfig config;
    NamespaceStats stats;
    int active; /* 1 = namespace attivo */

    /* Auth tokens associati a questo namespace */
    char **tokens;
    int token_count;
    int token_cap;

    /* Rate limiter per max_ops_per_sec */
    uint64_t ops_this_window;
    uint64_t window_start_us;
    pthread_mutex_t lock;
} Namespace;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * quota_init - Inizializza il subsistema multi-tenancy.
 * Returns: 0 su successo, -1 su errore.
 */
int quota_init(void);

/**
 * quota_shutdown - Libera tutte le risorse.
 */
void quota_shutdown(void);

/**
 * quota_namespace_create - Crea un nuovo namespace.
 * @config: Configurazione del namespace
 * Returns: 0 su successo, -1 se già esiste o errore.
 */
int quota_namespace_create(const NamespaceConfig *config);

/**
 * quota_namespace_update - Aggiorna la configurazione di un namespace.
 * Le modifiche sono applicate immediatamente (on-the-fly).
 */
int quota_namespace_update(const char *name, const char *key, const char *value);

/**
 * quota_namespace_drop - Elimina un namespace e tutti i suoi dati.
 * ATTENZIONE: operazione distruttiva, non reversibile.
 */
int quota_namespace_drop(const char *name);

/**
 * quota_namespace_evict - Forza eviction fino a liberare target_mb.
 * @name:      Nome namespace
 * @target_mb: MB da liberare
 * Returns: MB effettivamente liberati.
 */
size_t quota_namespace_evict(const char *name, size_t target_mb);

/**
 * quota_auth_token_add - Associa un token di auth a un namespace.
 * @ns_name: Namespace
 * @token:   Token (può essere un JWT o una stringa opaca)
 */
int quota_auth_token_add(const char *ns_name, const char *token);

/**
 * quota_resolve_namespace - Risolve il namespace da un token di auth.
 * @token:       Token AUTH del client
 * @out_name:    Buffer output con nome namespace
 * @out_cap:     Capacità buffer
 * Returns: 0 su successo, -1 se token non trovato.
 */
int quota_resolve_namespace(const char *token, char *out_name, size_t out_cap);

/**
 * quota_check - Verifica se un'operazione è consentita.
 * Chiamata automaticamente dal core engine prima di ogni comando.
 *
 * @ns_name:  Namespace
 * @bytes_in: Bytes della richiesta
 * Returns: 0 = consentita, 1 = throttled (attendi), -1 = rejected.
 */
int quota_check(const char *ns_name, size_t bytes_in);

/**
 * quota_record - Registra l'esecuzione di un comando.
 * Aggiorna stats e verifica violazioni.
 */
void quota_record(const char *ns_name,
                  size_t bytes_in,
                  size_t bytes_out,
                  double latency_us,
                  int is_hit);

/**
 * quota_namespace_stats - Statistiche di un namespace.
 */
NamespaceStats quota_namespace_stats(const char *name);

/**
 * quota_list_namespaces - Lista tutti i namespace attivi.
 * @out:      Array output nomi
 * @max:      Capacità array
 * Returns: numero di namespace.
 */
int quota_list_namespaces(char **out, int max);

/**
 * quota_print_stats - Stampa stats di tutti i namespace.
 */
void quota_print_all_stats(void);

#endif /* NEXCACHE_QUOTA_H */
