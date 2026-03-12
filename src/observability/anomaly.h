/* NexCache Anomaly Detection — Header
 * Prima assoluta: anomaly detection integrato nel core di un cache DB.
 * Nessun competitor (NexCache, NexCache, Dragonfly, KeyDB) ha questa feature.
 *
 * Anomalie rilevate automaticamente:
 *  1. Latency spike: latenza > 3x media ultimi 5 minuti
 *  2. Hot key emergence: chiave passa da 0 a 10k QPS in < 60s
 *  3. Memory leak: crescita > 10% in 10 min senza nuovi dati
 *  4. Eviction storm: eviction > 1000/sec per > 30s
 *  5. Connection surge: connessioni crescono > 5x in 30s
 *  6. Error rate spike: errori > 5% delle richieste per > 15s
 *
 * Azioni automatiche configurabili:
 *  - Log con contesto completo
 *  - Webhook alert
 *  - Rate limit automatico su hot key
 *  - Circuit breaker per connessioni anomale
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_ANOMALY_H
#define NEXCACHE_ANOMALY_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define ANOMALY_HISTORY_SIZE 300 /* 5 minuti a 1 campione/sec */
#define ANOMALY_MAX_EVENTS 1024  /* Max eventi salvati */
#define ANOMALY_WEBHOOK_MAX_URL 512

/* ── Tipo di anomalia ───────────────────────────────────────── */
typedef enum AnomalyType {
    ANOMALY_LATENCY_SPIKE = 0,
    ANOMALY_HOT_KEY = 1,
    ANOMALY_MEMORY_LEAK = 2,
    ANOMALY_EVICTION_STORM = 3,
    ANOMALY_CONNECTION_SURGE = 4,
    ANOMALY_ERROR_RATE = 5,
    ANOMALY_TYPE_COUNT = 6,
} AnomalyType;

/* ── Severità ───────────────────────────────────────────────── */
typedef enum AnomalySeverity {
    SEV_INFO = 0,
    SEV_WARNING = 1,
    SEV_CRITICAL = 2,
} AnomalySeverity;

/* ── Evento anomalia ────────────────────────────────────────── */
typedef struct AnomalyEvent {
    AnomalyType type;
    AnomalySeverity severity;
    uint64_t timestamp_us;
    double observed_value; /* Valore attuale */
    double baseline_value; /* Baseline storica */
    double ratio;          /* observed / baseline */
    char key[64];          /* Per hot_key: chiave coinvolta */
    char description[256];
    int auto_mitigated; /* 1 = mitigazione automatica applicata */
} AnomalyEvent;

/* ── Sliding window per una metrica ─────────────────────────── */
typedef struct MetricWindow {
    double samples[ANOMALY_HISTORY_SIZE];
    uint32_t head;
    uint32_t count;
    double sum;
    double min;
    double max;
} MetricWindow;

/* ── Hot key tracker: frequency sketch per frequenze ─────────── */
typedef struct HotKeyTracker {
    /* Count-Min Sketch: 4 hash functions × 1024 bucket */
    uint32_t sketch[4][1024];
    uint32_t total_ops;
    char top_key[64];
    uint32_t top_count;
} HotKeyTracker;

/* ── Configurazione soglie ──────────────────────────────────── */
typedef struct AnomalyConfig {
    double latency_spike_ratio;    /* Default 3.0 (3x baseline) */
    double hot_key_qps;            /* Default 10000 */
    double memory_growth_pct;      /* Default 10.0 (10%) */
    double eviction_storm_rate;    /* Default 1000/sec */
    double connection_surge_ratio; /* Default 5.0 */
    double error_rate_pct;         /* Default 5.0 */

    char webhook_url[ANOMALY_WEBHOOK_MAX_URL];
    int auto_ratelimit_hot_key; /* 1 = rate limit automatico hot key */
    int auto_circuit_breaker;   /* 1 = circuit breaker automatico */
} AnomalyConfig;

typedef struct AnomalyDetector {
    AnomalyConfig config;

    /* Windows per ogni metrica */
    MetricWindow latency_window;
    MetricWindow memory_window;
    MetricWindow eviction_window;
    MetricWindow connection_window;
    MetricWindow error_window;

    HotKeyTracker hot_key_tracker;

    /* Storico eventi */
    AnomalyEvent events[ANOMALY_MAX_EVENTS];
    uint32_t event_count;
    uint32_t event_head; /* Ring buffer */

    /* Thread di monitoraggio */
    pthread_t monitor_thread;
    pthread_mutex_t lock;
    int running;

    /* Callback per alert personalizzati */
    void (*alert_cb)(const AnomalyEvent *event, void *ctx);
    void *alert_ctx;

    /* Statistiche */
    uint64_t total_anomalies;
    uint64_t mitigations_applied;
} AnomalyDetector;

/* ── API ────────────────────────────────────────────────────── */
AnomalyDetector *anomaly_create(const AnomalyConfig *cfg,
                                void (*alert_cb)(const AnomalyEvent *, void *),
                                void *ctx);
void anomaly_destroy(AnomalyDetector *d);

/* Aggiorna metriche (chiamato dal core engine ogni secondo) */
void anomaly_update_latency(AnomalyDetector *d, double latency_us);
void anomaly_update_memory(AnomalyDetector *d, size_t used_bytes);
void anomaly_update_evictions(AnomalyDetector *d, uint64_t eviction_rate);
void anomaly_update_connections(AnomalyDetector *d, uint32_t active_conns);
void anomaly_update_errors(AnomalyDetector *d, double error_rate_pct);
void anomaly_record_key_access(AnomalyDetector *d, const char *key);

/* Analisi e rilevamento */
int anomaly_check(AnomalyDetector *d);

/* Stampa gli ultimi N eventi */
void anomaly_print_events(AnomalyDetector *d, uint32_t last_n);

/* Esporta eventi in JSON string */
int anomaly_export_json(AnomalyDetector *d, char *buf, size_t buf_len);

#endif /* NEXCACHE_ANOMALY_H */
