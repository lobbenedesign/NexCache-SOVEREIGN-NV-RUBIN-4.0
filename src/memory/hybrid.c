/* NexCache Hybrid RAM/SSD Manager — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "hybrid.h"
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

/* ── Struttura interna per mappa temperatura chiavi ─────────── */
#define TEMP_MAP_BUCKETS 65536

typedef struct TempEntry {
    char *key;
    size_t keylen;
    DataTemperature temp;
    struct TempEntry *next; /* Chaining per collisioni */
} TempEntry;

typedef struct TempMap {
    TempEntry **buckets;
    size_t count;
    pthread_mutex_t lock;
} TempMap;

/* Manager globale */
HybridManager *g_hybrid_manager = NULL;

/* Mappa temperatura locale */
static TempMap *g_temp_map = NULL;

/* ── Funzioni interne ───────────────────────────────────────── */

static uint64_t _us_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static uint32_t _hash_key(const char *key, size_t len) {
    /* FNV-1a hash veloce */
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)key[i];
        h *= 16777619U;
    }
    return h;
}

static TempMap *temp_map_create(void) {
    TempMap *map = (TempMap *)malloc(sizeof(TempMap));
    if (!map) return NULL;
    map->buckets = (TempEntry **)calloc(TEMP_MAP_BUCKETS, sizeof(TempEntry *));
    if (!map->buckets) {
        free(map);
        return NULL;
    }
    map->count = 0;
    pthread_mutex_init(&map->lock, NULL);
    return map;
}

static TempEntry *temp_map_get(TempMap *map, const char *key, size_t keylen) {
    uint32_t h = _hash_key(key, keylen) % TEMP_MAP_BUCKETS;
    TempEntry *e = map->buckets[h];
    while (e) {
        if (e->keylen == keylen && memcmp(e->key, key, keylen) == 0)
            return e;
        e = e->next;
    }
    return NULL;
}

static TempEntry *temp_map_get_or_create(TempMap *map,
                                         const char *key,
                                         size_t keylen) {
    uint32_t h = _hash_key(key, keylen) % TEMP_MAP_BUCKETS;
    TempEntry *e = map->buckets[h];
    while (e) {
        if (e->keylen == keylen && memcmp(e->key, key, keylen) == 0)
            return e;
        e = e->next;
    }
    /* Non trovato — crea */
    e = (TempEntry *)malloc(sizeof(TempEntry));
    if (!e) return NULL;
    e->key = (char *)malloc(keylen + 1);
    if (!e->key) {
        free(e);
        return NULL;
    }
    memcpy(e->key, key, keylen);
    e->key[keylen] = '\0';
    e->keylen = keylen;
    memset(&e->temp, 0, sizeof(DataTemperature));
    e->temp.temperature = 128; /* Temperatura iniziale media */
    e->next = map->buckets[h];
    map->buckets[h] = e;
    map->count++;
    return e;
}

/* ── Background thread: cooling + eviction ──────────────────── */
static void *hybrid_bg_thread(void *arg) {
    HybridManager *mgr = (HybridManager *)arg;

    while (mgr->running) {
        sleep(HYBRID_CHECK_INTERVAL);
        if (!mgr->running) break;

        uint64_t now_us = _us_now();
        size_t evicted_bytes = 0;

        pthread_mutex_lock(&g_temp_map->lock);

        for (int i = 0; i < TEMP_MAP_BUCKETS; i++) {
            TempEntry *e = g_temp_map->buckets[i];
            while (e) {
                /* Cooling: abbassa temperatura nel tempo */
                if (e->temp.temperature > TEMP_COOLING_RATE)
                    e->temp.temperature -= TEMP_COOLING_RATE;
                else
                    e->temp.temperature = 0;

                /* Reset contatore orario se >1 ora fa */
                uint64_t age_us = now_us - e->temp.last_access_us;
                if (age_us > 3600000000ULL) { /* 1 ora in µs */
                    e->temp.access_count_1h = 0;
                }

                /* Eviction: se freddo e non ancora su SSD */
                if (e->temp.temperature <= TEMP_COLD_THRESHOLD && !e->temp.on_ssd) {
                    /* Qui andrebbe la logica di serializzazione+scrittura SSD */
                    /* Per ora: solo marcatura + stats */
                    e->temp.on_ssd = 1;
                    mgr->stats.evictions++;
                    fprintf(stderr,
                            "[NexCache Hybrid] Evicted key '%s' (temp=%d) to SSD\n",
                            e->key, e->temp.temperature);
                }

                e = e->next;
            }
        }

        pthread_mutex_unlock(&g_temp_map->lock);

        /* Aggiorna stats RAM/SSD */
        pthread_mutex_lock(&mgr->lock);
        mgr->stats.ram_hit_rate = mgr->stats.ram_hits > 0 ? (double)mgr->stats.ram_hits /
                                                                (mgr->stats.ram_hits + mgr->stats.ssd_hits)
                                                          : 1.0;
        pthread_mutex_unlock(&mgr->lock);

        fprintf(stderr,
                "[NexCache Hybrid] Cooling pass: ram_hit_rate=%.1f%% "
                "evictions=%llu promotions=%llu\n",
                mgr->stats.ram_hit_rate * 100.0,
                (unsigned long long)mgr->stats.evictions,
                (unsigned long long)mgr->stats.promotions);
    }
    return NULL;
}

/* ── API pubblica ───────────────────────────────────────────── */

int hybrid_init(const char *ssd_path, size_t ssd_cache_size_mb, size_t ram_limit_mb) {
    if (g_hybrid_manager) return 0; /* Già inizializzato */

    HybridManager *mgr = (HybridManager *)calloc(1, sizeof(HybridManager));
    if (!mgr) return -1;

    strncpy(mgr->ssd_path, ssd_path ? ssd_path : "/tmp/nexcache_ssd.bin",
            sizeof(mgr->ssd_path) - 1);
    mgr->ssd_cache_size = ssd_cache_size_mb * 1024 * 1024;
    mgr->ram_limit = ram_limit_mb * 1024 * 1024;
    mgr->running = 1;

    /* Apri file SSD */
    mgr->ssd_fd = open(mgr->ssd_path,
                       O_CREAT | O_RDWR,
                       S_IRUSR | S_IWUSR);
    if (mgr->ssd_fd < 0) {
        fprintf(stderr, "[NexCache Hybrid] ERROR: cannot open SSD file '%s': %s\n",
                mgr->ssd_path, strerror(errno));
        free(mgr);
        return -1;
    }

    pthread_mutex_init(&mgr->lock, NULL);

    /* Crea mappa temperatura */
    g_temp_map = temp_map_create();
    if (!g_temp_map) {
        close(mgr->ssd_fd);
        free(mgr);
        return -1;
    }

    g_hybrid_manager = mgr;

    /* Avvia background thread */
    pthread_create(&mgr->bg_thread, NULL, hybrid_bg_thread, mgr);

    fprintf(stderr,
            "[NexCache Hybrid] Initialized: ssd='%s' ssd_limit=%zuMB ram_limit=%zuMB\n",
            mgr->ssd_path,
            ssd_cache_size_mb,
            ram_limit_mb);

    return 0;
}

void hybrid_update_temperature(const char *key, size_t keylen) {
    if (!g_hybrid_manager || !g_temp_map) return;

    pthread_mutex_lock(&g_temp_map->lock);
    TempEntry *e = temp_map_get_or_create(g_temp_map, key, keylen);
    if (e) {
        e->temp.last_access_us = _us_now();
        e->temp.access_count_24h++;
        e->temp.access_count_1h++;

        /* Aumenta temperatura, cap a 255 */
        int new_temp = (int)e->temp.temperature + 20;
        e->temp.temperature = (uint8_t)(new_temp > 255 ? 255 : new_temp);

        /* Se il dato era su SSD e ora è caldo → promovi */
        if (e->temp.on_ssd && e->temp.temperature >= TEMP_HOT_THRESHOLD) {
            /* Promozione automatica */
            e->temp.on_ssd = 0;
            g_hybrid_manager->stats.promotions++;
            g_hybrid_manager->stats.ssd_hits++;
        } else {
            g_hybrid_manager->stats.ram_hits++;
        }
    }
    pthread_mutex_unlock(&g_temp_map->lock);
}

ssize_t hybrid_evict_cold_data(size_t target_free_mb) {
    if (!g_hybrid_manager || !g_temp_map) return -1;

    size_t freed = 0;
    size_t target = target_free_mb * 1024 * 1024;

    pthread_mutex_lock(&g_temp_map->lock);
    for (int i = 0; i < TEMP_MAP_BUCKETS && freed < target; i++) {
        TempEntry *e = g_temp_map->buckets[i];
        while (e && freed < target) {
            if (e->temp.temperature <= TEMP_COLD_THRESHOLD && !e->temp.on_ssd) {
                e->temp.on_ssd = 1;
                g_hybrid_manager->stats.evictions++;
                freed += 256; /* Stima conservativa */
            }
            e = e->next;
        }
    }
    pthread_mutex_unlock(&g_temp_map->lock);

    return (ssize_t)(freed / 1024 / 1024);
}

int hybrid_promote_hot_data(const char *key, size_t keylen) {
    if (!g_hybrid_manager || !g_temp_map) return -1;

    pthread_mutex_lock(&g_temp_map->lock);
    TempEntry *e = temp_map_get(g_temp_map, key, keylen);
    int result = -1;
    if (e && e->temp.on_ssd) {
        e->temp.on_ssd = 0;
        e->temp.temperature = TEMP_HOT_THRESHOLD;
        g_hybrid_manager->stats.promotions++;
        result = 0;
    }
    pthread_mutex_unlock(&g_temp_map->lock);
    return result;
}

int hybrid_get_temperature(const char *key, size_t keylen, DataTemperature *out) {
    if (!g_temp_map || !out) return -1;

    pthread_mutex_lock(&g_temp_map->lock);
    TempEntry *e = temp_map_get(g_temp_map, key, keylen);
    if (e) {
        *out = e->temp;
        pthread_mutex_unlock(&g_temp_map->lock);
        return 0;
    }
    pthread_mutex_unlock(&g_temp_map->lock);
    return -1;
}

int hybrid_is_on_ssd(const char *key, size_t keylen) {
    if (!g_temp_map) return 0;
    pthread_mutex_lock(&g_temp_map->lock);
    TempEntry *e = temp_map_get(g_temp_map, key, keylen);
    int result = (e && e->temp.on_ssd) ? 1 : 0;
    pthread_mutex_unlock(&g_temp_map->lock);
    return result;
}

HybridStats hybrid_get_stats(void) {
    HybridStats s = {0};
    if (g_hybrid_manager) {
        pthread_mutex_lock(&g_hybrid_manager->lock);
        s = g_hybrid_manager->stats;
        pthread_mutex_unlock(&g_hybrid_manager->lock);
    }
    return s;
}

void hybrid_print_stats(void) {
    if (!g_hybrid_manager) return;
    HybridStats s = hybrid_get_stats();
    fprintf(stderr,
            "[NexCache Hybrid Stats]\n"
            "  RAM: %llu MB | SSD: %llu MB\n"
            "  Evictions: %llu | Promotions: %llu\n"
            "  RAM hits: %llu | SSD hits: %llu\n"
            "  RAM hit rate: %.1f%%\n"
            "  Avg SSD latency: %.1f µs\n",
            (unsigned long long)(s.ram_bytes / 1024 / 1024),
            (unsigned long long)(s.ssd_bytes / 1024 / 1024),
            (unsigned long long)s.evictions,
            (unsigned long long)s.promotions,
            (unsigned long long)s.ram_hits,
            (unsigned long long)s.ssd_hits,
            s.ram_hit_rate * 100.0,
            s.avg_ssd_latency_us);
}

void hybrid_shutdown(void) {
    if (!g_hybrid_manager) return;
    g_hybrid_manager->running = 0;
    pthread_join(g_hybrid_manager->bg_thread, NULL);
    if (g_hybrid_manager->ssd_fd >= 0)
        close(g_hybrid_manager->ssd_fd);
    hybrid_print_stats();
    pthread_mutex_destroy(&g_hybrid_manager->lock);
    free(g_hybrid_manager);
    g_hybrid_manager = NULL;
}
