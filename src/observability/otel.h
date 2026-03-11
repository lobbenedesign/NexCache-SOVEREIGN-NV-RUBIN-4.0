/* NexCache Observability Layer — MODULO 11 (v2.0)
 * ============================================================
 * Né Redis né Valkey hanno osservabilità integrata nel core.
 * Serve sempre un tool esterno (Redis Insight, BetterDB, ecc.)
 *
 * NexCache integra tutto nativamente:
 *   - OpenTelemetry tracing per ogni comando
 *   - Prometheus metrics senza exporter aggiuntivi
 *   - Dashboard web embedded (porta 8080, zero config)
 *   - Hot key detection real-time (come Redis 8.6 HOTKEYS)
 *   - Query flamegraph per slow query analysis
 *   - Per-worker statistics con latenza P50/P99/P999
 *
 * Nessuna installazione richiesta. Zero configurazione.
 * Aprire un browser su http://nexcache-host:8080
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_OBSERVABILITY_H
#define NEXCACHE_OBSERVABILITY_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdatomic.h>

/* ── Configurazione ─────────────────────────────────────────── */
#define OBS_DASHBOARD_PORT 8080     /* Porta dashboard web */
#define OBS_METRICS_PATH "/metrics" /* Prometheus endpoint */
#define OBS_HEALTH_PATH "/health"
#define OBS_TRACES_PATH "/traces"
#define OBS_HOTKEYS_PATH "/hotkeys"
#define OBS_SLOW_PATH "/slow"
#define OBS_WORKERS_PATH "/workers"

#define OBS_TOP_HOTKEYS 100               /* Top N chiavi più accedute */
#define OBS_SLOW_QUERY_THRESHOLD_US 10000 /* Soglia slow query: 10ms */
#define OBS_SLOW_QUERY_MAX_LOG 1000       /* Max slow query nel log */
#define OBS_SPAN_RING_SIZE 65536          /* Ring buffer per spans OTel */

/* ── OpenTelemetry Span ─────────────────────────────────────── */
/*
 * Ogni comando NexCache genera automaticamente un OTel span con:
 *   - nexcache.command:    nome comando (GET, SET, VSIM, ecc.)
 *   - nexcache.key:        chiave (con PII masking opzionale)
 *   - nexcache.worker_id:  worker che ha processato
 *   - nexcache.latency_us: latenza in microsecondi
 *   - nexcache.hit:        true se cache hit
 *   - nexcache.algo:       algoritmo usato (per vector search)
 *   - nexcache.bytes_in:   bytes in input
 *   - nexcache.bytes_out:  bytes in output
 */
typedef struct OTelSpan {
    char trace_id[33];       /* 128-bit hex string */
    char span_id[17];        /* 64-bit hex string */
    char parent_span_id[17]; /* ID span padre (se nested) */
    char service_name[64];   /* "nexcache" */
    char command[64];        /* Nome comando */
    char key[256];           /* Chiave (mascherata se PII) */
    uint32_t worker_id;      /* Worker che ha eseguito */
    uint64_t start_us;       /* Timestamp inizio */
    uint64_t end_us;         /* Timestamp fine */
    uint64_t latency_us;     /* Latenza calcolata */
    uint32_t bytes_in;       /* Bytes richiesta */
    uint32_t bytes_out;      /* Bytes risposta */
    uint8_t cache_hit;       /* 1 = hit */
    uint8_t is_error;        /* 1 = errore */
    char error_msg[256];     /* Messaggio errore */
    char vector_algo[32];    /* Algo usato se VSIM */
} OTelSpan;

/* ── Hot Key tracking ───────────────────────────────────────── */
typedef struct HotKey {
    char key[512];         /* Chiave */
    uint64_t access_count; /* Accessi totali */
    uint64_t access_1m;    /* Accessi ultimo minuto */
    uint64_t access_1h;    /* Accessi ultima ora */
    double ops_per_sec;    /* Ops/sec corrente */
    uint8_t is_write;      /* 1 = prevalentemente write */
    int shard_id;          /* Shard che la gestisce */
} HotKey;

/* ── Slow Query Log ─────────────────────────────────────────── */
typedef struct SlowQuery {
    char command[64];
    char key[512];
    uint64_t latency_us;
    uint64_t timestamp_us;
    uint32_t worker_id;
    char client_ip[46];
} SlowQuery;

/* ── Prometheus Metric ──────────────────────────────────────── */
typedef enum MetricType {
    METRIC_COUNTER = 0,
    METRIC_GAUGE = 1,
    METRIC_HISTOGRAM = 2,
    METRIC_SUMMARY = 3,
} MetricType;

/* ── Statistiche globali per /metrics ──────────────────────── */
typedef struct NexMetrics {
    /* Comandi */
    uint64_t total_commands;
    uint64_t commands_per_sec;
    uint64_t total_errors;
    uint64_t total_hits;
    uint64_t total_misses;
    double hit_rate;

    /* Latenza */
    double latency_p50_us;
    double latency_p99_us;
    double latency_p999_us;
    double latency_max_us;

    /* Connessioni */
    uint32_t connected_clients;
    uint32_t total_connections;
    uint64_t bytes_received;
    uint64_t bytes_sent;

    /* Memoria */
    uint64_t used_memory_bytes;
    uint64_t peak_memory_bytes;
    double memory_fragmentation;
    uint64_t arena_allocated_bytes;
    uint64_t arena_wasted_bytes;

    /* Workers */
    uint32_t active_workers;
    double worker_utilization_avg;
    double worker_utilization_max;

    /* Vector search */
    uint64_t vsim_queries;
    double vsim_p99_latency_ms;
    double vsim_avg_recall;

    /* SSD tiering */
    uint64_t ssd_bytes_used;
    uint64_t ssd_promotions;
    uint64_t ssd_evictions;

    /* Replica */
    uint32_t replica_lag_ms;
    char consensus_mode[16];
    int is_leader;
    uint32_t leader_id;
} NexMetrics;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * obs_init - Inizializza il layer di osservabilità.
 * @dashboard_port: Porta del dashboard web (0 = usa default 8080)
 * @enable_pii_masking: 1 = maschera le chiavi nei log (es. user:XXX)
 * @otlp_endpoint: URL esportatore OTLP (NULL = disabilitato)
 *
 * Returns: 0 su successo, -1 su errore.
 */
int obs_init(uint16_t dashboard_port,
             int enable_pii_masking,
             const char *otlp_endpoint);

/**
 * obs_shutdown - Ferma il dashboard e flush dei traces.
 */
void obs_shutdown(void);

/**
 * obs_record_command - Registra l'esecuzione di un comando.
 * Chiamato automaticamente dal core engine dopo ogni comando.
 * Thread-safe, lock-free per le operazioni comuni.
 */
void obs_record_command(const OTelSpan *span);

/**
 * obs_hotkey_access - Aggiorna il contatore di una hot key.
 * Chiamato automaticamente ad ogni GET/SET.
 */
void obs_hotkey_access(const char *key, size_t keylen, int is_write);

/**
 * obs_get_hotkeys - Lista delle N chiavi più accedute.
 * @topn:     Numero di hot key da restituire
 * @out:      Array output
 * @out_cap:  Capacità array
 * Returns: numero di hot key restituite.
 */
int obs_get_hotkeys(int topn, HotKey *out, int out_cap);

/**
 * obs_get_slow_queries - Lista delle query più lente.
 * @out:    Array output
 * @max:    Numero max di risultati
 * Returns: numero di slow query.
 */
int obs_get_slow_queries(SlowQuery *out, int max);

/**
 * obs_get_metrics - Snapshot delle metriche correnti.
 */
NexMetrics obs_get_metrics(void);

/**
 * obs_metrics_to_prometheus - Serializza in formato Prometheus.
 * @buf:  Buffer output
 * @cap:  Capacità buffer
 * Returns: bytes scritti.
 */
ssize_t obs_metrics_to_prometheus(char *buf, size_t cap);

/**
 * obs_metrics_to_json - Serializza metriche in JSON.
 */
ssize_t obs_metrics_to_json(char *buf, size_t cap);

/**
 * obs_flush_traces - Forza l'invio dei traces all'OTLP exporter.
 */
int obs_flush_traces(void);

/* ── PII Masking ────────────────────────────────────────────── */
/*
 * Se enable_pii_masking=1, le chiavi vengono mascherata nei log:
 * "user:123456" → "user:XXX"
 * "session:abc" → "session:XXX"
 * "email:mario@example.com" → "email:XXX"
 *
 * Il masking avviene SOLO nel logging/tracing, non nella risposta.
 * Configurabile con pattern regex nel nexcache.conf:
 * obs-pii-pattern "user:*"
 * obs-pii-pattern "email:*"
 * obs-pii-pattern "phone:*"
 */

/**
 * obs_mask_key - Applica PII masking a una chiave.
 * @key:    Chiave originale
 * @out:    Buffer output (chiave mascherata)
 * @cap:    Capacità buffer
 */
void obs_mask_key(const char *key, char *out, size_t cap);

#endif /* NEXCACHE_OBSERVABILITY_H */
