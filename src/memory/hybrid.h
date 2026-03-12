/* NexCache Hybrid RAM/SSD Manager — MODULO 5
 * ============================================================
 * Sposta automaticamente i dati "freddi" su SSD NVMe.
 * I dati caldi restano in RAM. Latenza accesso SSD: <1ms.
 *
 * NexCache tiene TUTTO in RAM (costosa).
 * NexCache riduce il costo infrastruttura del 60-80%.
 *
 * Algoritmo temperatura:
 *   - Ogni chiave ha un contatore temperatura (0-255)
 *   - Ogni accesso incrementa la temperatura
 *   - Un background thread decrementa periodicamente (cooling)
 *   - Dati sotto soglia COLD → serializzati su SSD
 *   - Accesso a dato su SSD → deserializzazione + promozione RAM
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_HYBRID_H
#define NEXCACHE_HYBRID_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <pthread.h>
#include "arena.h"

/* ── Soglie temperatura ─────────────────────────────────────── */
#define TEMP_HOT_THRESHOLD 200 /* >= 200: sempre in RAM */
#define TEMP_WARM_THRESHOLD 50 /* 50-199: in RAM, candidato SSD */
#define TEMP_COLD_THRESHOLD 10 /* <= 10: sposta su SSD */

/* Quante volte la temperatura cala per ciclo di cooling */
#define TEMP_COOLING_RATE 5

/* Intervallo controllo background thread (secondi) */
#define HYBRID_CHECK_INTERVAL 30

/* Dimensione massima di un oggetto che va su SSD (bytes) */
#define HYBRID_MAX_OBJECT_SIZE (256UL * 1024 * 1024) /* 256MB */

/* ── Struttura temperatura di un oggetto ────────────────────── */
typedef struct DataTemperature {
    uint64_t last_access_us;   /* Timestamp ultimo accesso µs */
    uint32_t access_count_24h; /* Accessi nelle ultime 24 ore */
    uint16_t access_count_1h;  /* Accessi nell'ultima ora */
    uint8_t temperature;       /* Temperatura corrente 0-255 */
    uint8_t on_ssd;            /* 1 = il dato è su SSD, 0 = in RAM */
    uint64_t ssd_offset;       /* Offset nel file SSD (se on_ssd=1) */
    uint32_t ssd_size;         /* Dimensione su SSD (compresso) */
} DataTemperature;

/* ── Statistiche manager ────────────────────────────────────── */
typedef struct HybridStats {
    uint64_t ram_bytes;        /* Bytes totali in RAM */
    uint64_t ssd_bytes;        /* Bytes totali su SSD */
    uint64_t promotions;       /* Oggetti SSD → RAM */
    uint64_t evictions;        /* Oggetti RAM → SSD */
    uint64_t ram_hits;         /* Hit in RAM */
    uint64_t ssd_hits;         /* Hit su SSD */
    double avg_ssd_latency_us; /* Latenza media accesso SSD (µs) */
    double ram_hit_rate;       /* Hit rate RAM (calcolato) */
} HybridStats;

/* ── Struttura principale Hybrid Manager ────────────────────── */
typedef struct HybridManager {
    /* Configurazione */
    char ssd_path[512];    /* Path del file SSD cache */
    int ssd_fd;            /* File descriptor SSD */
    size_t ssd_cache_size; /* Dimensione massima cache SSD */
    size_t ssd_used;       /* Bytes usati su SSD */
    size_t ram_limit;      /* Limite massimo RAM (0 = auto) */

    /* Thread background per cooling/eviction */
    pthread_t bg_thread;
    pthread_mutex_t lock;
    int running; /* Flag per stop del thread */

    /* Statistiche */
    HybridStats stats;

    /* Arena per le strutture interne del manager */
    Arena *arena;
} HybridManager;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * hybrid_init - Inizializza il manager ibrido RAM/SSD.
 * @ssd_path: Path del file usato come cache SSD
 * @ssd_cache_size_mb: Dimensione massima cache SSD in MB
 * @ram_limit_mb: Limite RAM in MB (0 = usa 80% della RAM disponibile)
 *
 * Returns: 0 su successo, -1 su errore.
 */
int hybrid_init(const char *ssd_path,
                size_t ssd_cache_size_mb,
                size_t ram_limit_mb);

/**
 * hybrid_shutdown - Ferma il manager, flush su SSD, cleanup.
 */
void hybrid_shutdown(void);

/**
 * hybrid_update_temperature - Chiama a ogni accesso a una chiave.
 * Incrementa la temperatura e aggiorna il timestamp.
 *
 * @key:    Chiave acceduta
 * @keylen: Lunghezza chiave
 */
void hybrid_update_temperature(const char *key, size_t keylen);

/**
 * hybrid_evict_cold_data - Sposta i dati freddi su SSD.
 * @target_free_mb: Quanta RAM liberare
 *
 * Returns: MB effettivamente liberati, -1 su errore.
 */
ssize_t hybrid_evict_cold_data(size_t target_free_mb);

/**
 * hybrid_promote_hot_data - Carica un oggetto da SSD in RAM.
 * @key:    Chiave da promuovere
 * @keylen: Lunghezza chiave
 *
 * Returns: 0 su successo, -1 su errore o chiave non su SSD.
 */
int hybrid_promote_hot_data(const char *key, size_t keylen);

/**
 * hybrid_get_temperature - Ottieni la temperatura di una chiave.
 * @key:    Chiave
 * @keylen: Lunghezza chiave
 * @out:    Output — struct DataTemperature popolata
 *
 * Returns: 0 su successo, -1 se chiave non trovata.
 */
int hybrid_get_temperature(const char *key, size_t keylen, DataTemperature *out);

/**
 * hybrid_get_stats - Statistiche correnti del manager.
 */
HybridStats hybrid_get_stats(void);

/**
 * hybrid_print_stats - Stampa statistiche su stderr.
 */
void hybrid_print_stats(void);

/**
 * hybrid_is_on_ssd - True se la chiave è attualmente su SSD.
 */
int hybrid_is_on_ssd(const char *key, size_t keylen);

/* ── Puntatore al manager globale ───────────────────────────── */
extern HybridManager *g_hybrid_manager;

#endif /* NEXCACHE_HYBRID_H */
