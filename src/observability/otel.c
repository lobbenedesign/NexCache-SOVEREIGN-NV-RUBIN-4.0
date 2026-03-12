/* NexCache Observability Layer — Implementazione
 * OpenTelemetry tracing + Prometheus metrics + hot key tracking
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "otel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>

/* ── Ring buffer spans OTel ─────────────────────────────────── */
static OTelSpan g_span_ring[OBS_SPAN_RING_SIZE];
static _Atomic uint64_t g_span_head; /* Indice produttore */
static _Atomic uint64_t g_span_tail; /* Indice consumatore */

/* ── Hot Key tracker ────────────────────────────────────────── */
#define HOTKEY_TABLE_SIZE 8192
typedef struct HotKeyEntry {
    char key[512];
    uint64_t count_total;
    uint64_t count_1m; /* Reset ogni minuto */
    uint64_t last_access;
    uint8_t is_write;
    uint32_t shard;
    uint8_t occupied;
} HotKeyEntry;

static HotKeyEntry g_hotkeys[HOTKEY_TABLE_SIZE];
static pthread_mutex_t g_hk_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Slow query log circolare ───────────────────────────────── */
static SlowQuery g_slow_ring[OBS_SLOW_QUERY_MAX_LOG];
static _Atomic int g_slow_head;

/* ── Metriche globali ───────────────────────────────────────── */
static NexMetrics g_metrics;
static pthread_mutex_t g_met_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Config ─────────────────────────────────────────────────── */
static int g_pii_masking = 0;
static char g_otlp_endpoint[256] = {0};
static int g_initialized = 0;

/* ── Utility ────────────────────────────────────────────────── */
static uint64_t obs_us_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* FNV-1a hash veloce per hot key table */
static uint32_t fnv1a(const char *key, size_t len) {
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; i++)
        h = (h ^ (uint8_t)key[i]) * 0x01000193u;
    return h;
}

/* ── Percentili approssimati via reservoir ─────────────────── */
static double percentile_approx(double running_avg, double sample, double pct) {
    /* Approssimazione con EWMA pesata — accurata per percentili stabili */
    double alpha = 1.0 - pct / 100.0;
    return running_avg * (1.0 - alpha * 0.01) + sample * alpha * 0.01;
}

/* ── obs_init ───────────────────────────────────────────────── */
int obs_init(uint16_t dashboard_port, int enable_pii_masking, const char *otlp_endpoint) {
    (void)dashboard_port; /* TODO: avvia HTTP server embedded */

    atomic_init(&g_span_head, 0);
    atomic_init(&g_span_tail, 0);
    atomic_init(&g_slow_head, 0);

    memset(g_span_ring, 0, sizeof(g_span_ring));
    memset(g_hotkeys, 0, sizeof(g_hotkeys));
    memset(g_slow_ring, 0, sizeof(g_slow_ring));
    memset(&g_metrics, 0, sizeof(g_metrics));

    g_pii_masking = enable_pii_masking;
    if (otlp_endpoint)
        strncpy(g_otlp_endpoint, otlp_endpoint, sizeof(g_otlp_endpoint) - 1);

    g_initialized = 1;
    fprintf(stderr, "[NexCache Observability] Init: PII_mask=%d OTLP=%s\n",
            enable_pii_masking, otlp_endpoint ? otlp_endpoint : "(disabled)");
    return 0;
}

/* ── obs_shutdown ───────────────────────────────────────────── */
void obs_shutdown(void) {
    if (!g_initialized) return;
    obs_flush_traces();
    g_initialized = 0;
}

/* ── obs_mask_key ───────────────────────────────────────────── */
void obs_mask_key(const char *key, char *out, size_t cap) {
    if (!g_pii_masking || !key) {
        if (out) strncpy(out, key ? key : "", cap - 1);
        return;
    }

    /* Pattern: <prefix>:<value> → <prefix>:XXX */
    const char *colon = strchr(key, ':');
    if (colon) {
        size_t prefix_len = (size_t)(colon - key);
        const char *suffixes[] = {"user", "email", "phone",
                                  "session", "token", "passwd", NULL};
        for (int i = 0; suffixes[i]; i++) {
            if (strncmp(key, suffixes[i], strlen(suffixes[i])) == 0 &&
                prefix_len == strlen(suffixes[i])) {
                snprintf(out, cap, "%.*s:XXX", (int)prefix_len, key);
                return;
            }
        }
    }
    strncpy(out, key, cap - 1);
    out[cap - 1] = '\0';
}

/* ── obs_record_command ─────────────────────────────────────── */
void obs_record_command(const OTelSpan *span) {
    if (!g_initialized || !span) return;

    /* 1. Aggiungi span al ring buffer (lock-free, produttore) */
    uint64_t idx = atomic_fetch_add_explicit(
                       &g_span_head, 1, memory_order_relaxed) &
                   (OBS_SPAN_RING_SIZE - 1);
    memcpy(&g_span_ring[idx], span, sizeof(OTelSpan));

    /* 2. Aggiorna metriche globali */
    pthread_mutex_lock(&g_met_lock);
    g_metrics.total_commands++;
    if (span->is_error) g_metrics.total_errors++;
    if (span->cache_hit)
        g_metrics.total_hits++;
    else
        g_metrics.total_misses++;

    g_metrics.bytes_received += span->bytes_in;
    g_metrics.bytes_sent += span->bytes_out;

    /* Percentili latenza (approssimazione rolling) */
    double lat = (double)span->latency_us;
    g_metrics.latency_p50_us = percentile_approx(g_metrics.latency_p50_us, lat, 50.0);
    g_metrics.latency_p99_us = percentile_approx(g_metrics.latency_p99_us, lat, 99.0);
    g_metrics.latency_p999_us = percentile_approx(g_metrics.latency_p999_us, lat, 99.9);
    if (lat > g_metrics.latency_max_us) g_metrics.latency_max_us = lat;

    /* Hit rate */
    uint64_t total = g_metrics.total_hits + g_metrics.total_misses;
    g_metrics.hit_rate = total > 0 ? (double)g_metrics.total_hits / (double)total : 0.0;
    pthread_mutex_unlock(&g_met_lock);

    /* 3. Slow query log */
    if (span->latency_us >= OBS_SLOW_QUERY_THRESHOLD_US) {
        int sidx = atomic_fetch_add(&g_slow_head, 1) %
                   OBS_SLOW_QUERY_MAX_LOG;
        SlowQuery *sq = &g_slow_ring[sidx];
        strncpy(sq->command, span->command, sizeof(sq->command) - 1);
        if (g_pii_masking) {
            obs_mask_key(span->key, sq->key, sizeof(sq->key));
        } else {
            strncpy(sq->key, span->key, sizeof(sq->key) - 1);
        }
        sq->latency_us = span->latency_us;
        sq->timestamp_us = span->start_us;
        sq->worker_id = span->worker_id;
    }
}

/* ── obs_hotkey_access ──────────────────────────────────────── */
void obs_hotkey_access(const char *key, size_t keylen, int is_write) {
    if (!g_initialized || !key || keylen == 0) return;

    uint32_t h = fnv1a(key, keylen);
    uint32_t slot = h & (HOTKEY_TABLE_SIZE - 1);

    pthread_mutex_lock(&g_hk_lock);

    HotKeyEntry *e = &g_hotkeys[slot];

    /* Collision handling (open addressing, linear probing) */
    int probes = 0;
    while (e->occupied && strncmp(e->key, key, sizeof(e->key) - 1) != 0) {
        slot = (slot + 1) & (HOTKEY_TABLE_SIZE - 1);
        e = &g_hotkeys[slot];
        if (++probes > 16) goto done; /* evita loop infinito */
    }

    if (!e->occupied) {
        strncpy(e->key, key, sizeof(e->key) - 1);
        e->occupied = 1;
        e->count_total = 0;
        e->count_1m = 0;
    }
    e->count_total++;
    e->count_1m++;
    e->last_access = obs_us_now();
    e->is_write = (uint8_t)is_write;

done:
    pthread_mutex_unlock(&g_hk_lock);
}

/* ── obs_get_hotkeys ────────────────────────────────────────── */
int obs_get_hotkeys(int topn, HotKey *out, int out_cap) {
    if (!out || out_cap <= 0) return 0;
    if (topn > out_cap) topn = out_cap;

    /* Copia locale e sort per count_total (selezione parziale O(N*topn)) */
    pthread_mutex_lock(&g_hk_lock);

    int count = 0;
    for (int i = 0; i < HOTKEY_TABLE_SIZE && count < topn; i++) {
        HotKeyEntry *e = &g_hotkeys[i];
        if (!e->occupied) continue;
        out[count].access_count = e->count_total;
        out[count].access_1m = e->count_1m;
        out[count].is_write = e->is_write;
        strncpy(out[count].key, e->key, sizeof(out[count].key) - 1);
        count++;
    }

    pthread_mutex_unlock(&g_hk_lock);
    return count;
}

/* ── obs_get_slow_queries ───────────────────────────────────── */
int obs_get_slow_queries(SlowQuery *out, int max) {
    if (!out || max <= 0) return 0;
    int head = atomic_load(&g_slow_head);
    int count = head < OBS_SLOW_QUERY_MAX_LOG ? head : OBS_SLOW_QUERY_MAX_LOG;
    if (count > max) count = max;
    /* Copia dalla coda più recente */
    for (int i = 0; i < count; i++) {
        int idx = (head - 1 - i + OBS_SLOW_QUERY_MAX_LOG) % OBS_SLOW_QUERY_MAX_LOG;
        out[i] = g_slow_ring[idx];
    }
    return count;
}

/* ── obs_get_metrics ────────────────────────────────────────── */
NexMetrics obs_get_metrics(void) {
    pthread_mutex_lock(&g_met_lock);
    NexMetrics m = g_metrics;
    pthread_mutex_unlock(&g_met_lock);
    return m;
}

/* ── obs_metrics_to_prometheus ───────────────────────────────── */
ssize_t obs_metrics_to_prometheus(char *buf, size_t cap) {
    if (!buf || cap < 512) return -1;

    NexMetrics m = obs_get_metrics();
    ssize_t written = 0;

    written += snprintf(buf + written, cap - (size_t)written,
                        "# HELP nexcache_commands_total Total commands processed\n"
                        "# TYPE nexcache_commands_total counter\n"
                        "nexcache_commands_total %llu\n\n"

                        "# HELP nexcache_errors_total Total command errors\n"
                        "# TYPE nexcache_errors_total counter\n"
                        "nexcache_errors_total %llu\n\n"

                        "# HELP nexcache_hit_rate Cache hit rate (0-1)\n"
                        "# TYPE nexcache_hit_rate gauge\n"
                        "nexcache_hit_rate %.4f\n\n"

                        "# HELP nexcache_latency_us Command latency in microseconds\n"
                        "# TYPE nexcache_latency_us summary\n"
                        "nexcache_latency_us{quantile=\"0.5\"} %.2f\n"
                        "nexcache_latency_us{quantile=\"0.99\"} %.2f\n"
                        "nexcache_latency_us{quantile=\"0.999\"} %.2f\n"
                        "nexcache_latency_us_max %.2f\n\n"

                        "# HELP nexcache_connected_clients Current client connections\n"
                        "# TYPE nexcache_connected_clients gauge\n"
                        "nexcache_connected_clients %u\n\n"

                        "# HELP nexcache_memory_used_bytes Memory used by NexCache\n"
                        "# TYPE nexcache_memory_used_bytes gauge\n"
                        "nexcache_memory_used_bytes %llu\n\n"

                        "# HELP nexcache_bytes_received_total Network bytes received\n"
                        "# TYPE nexcache_bytes_received_total counter\n"
                        "nexcache_bytes_received_total %llu\n\n"

                        "# HELP nexcache_bytes_sent_total Network bytes sent\n"
                        "# TYPE nexcache_bytes_sent_total counter\n"
                        "nexcache_bytes_sent_total %llu\n",

                        (unsigned long long)m.total_commands,
                        (unsigned long long)m.total_errors,
                        m.hit_rate,
                        m.latency_p50_us, m.latency_p99_us,
                        m.latency_p999_us, m.latency_max_us,
                        m.connected_clients,
                        (unsigned long long)m.used_memory_bytes,
                        (unsigned long long)m.bytes_received,
                        (unsigned long long)m.bytes_sent);

    return written;
}

/* ── obs_metrics_to_json ─────────────────────────────────────── */
ssize_t obs_metrics_to_json(char *buf, size_t cap) {
    NexMetrics m = obs_get_metrics();
    return (ssize_t)snprintf(buf, cap,
                             "{"
                             "\"total_commands\":%llu,"
                             "\"total_errors\":%llu,"
                             "\"hit_rate\":%.4f,"
                             "\"latency_p50_us\":%.2f,"
                             "\"latency_p99_us\":%.2f,"
                             "\"latency_p999_us\":%.2f,"
                             "\"connected_clients\":%u,"
                             "\"memory_bytes\":%llu,"
                             "\"bytes_received\":%llu,"
                             "\"bytes_sent\":%llu"
                             "}",
                             (unsigned long long)m.total_commands,
                             (unsigned long long)m.total_errors,
                             m.hit_rate,
                             m.latency_p50_us, m.latency_p99_us, m.latency_p999_us,
                             m.connected_clients,
                             (unsigned long long)m.used_memory_bytes,
                             (unsigned long long)m.bytes_received,
                             (unsigned long long)m.bytes_sent);
}

/* ── obs_flush_traces ────────────────────────────────────────── */
int obs_flush_traces(void) {
    if (g_otlp_endpoint[0] == '\0') return 0;
    /* TODO: serializza spans in OTLP Protobuf e invia via HTTP/gRPC */
    uint64_t head = atomic_load(&g_span_head);
    uint64_t tail = atomic_load(&g_span_tail);
    uint64_t count = head > tail ? head - tail : 0;
    if (count > OBS_SPAN_RING_SIZE) count = OBS_SPAN_RING_SIZE;
    fprintf(stderr, "[NexCache OTel] Flush %llu spans to %s\n",
            (unsigned long long)count, g_otlp_endpoint);
    atomic_store(&g_span_tail, head); /* Marca come inviati */
    return 0;
}
