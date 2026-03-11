/* NexSegcache — Implementazione
 * Ispirato a Segcache di CMU (NSDI'21 Best Paper Award).
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "segcache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t seg_unix_now(void) {
    return (uint32_t)time(NULL);
}

/* ── FNV-1a 64 bit per hash table ───────────────────────────── */
static uint64_t seg_hash(const char *key, uint16_t key_len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (uint16_t i = 0; i < key_len; i++)
        h = (h ^ (uint8_t)key[i]) * 0x100000001b3ULL;
    return h;
}

/* TTL → bucket index: raggruppa TTL simili nello stesso bucket
 * Granularità progressiva: 1s per TTL<60s, 10s per TTL<600s, ecc. */
static uint32_t ttl_to_bucket(uint32_t ttl_seconds) {
    if (ttl_seconds == 0) return 0;
    if (ttl_seconds < 60) return ttl_seconds;
    if (ttl_seconds < 600) return 60 + (ttl_seconds - 60) / 10;
    if (ttl_seconds < 3600) return 60 + 54 + (ttl_seconds - 600) / 60;
    return (60 + 54 + 50 + (ttl_seconds - 3600) / 3600) % SEG_TTL_BUCKETS;
}

/* ── segcache_create ─────────────────────────────────────────── */
NexSegcache *segcache_create(size_t max_memory, uint32_t segment_size) {
    if (segment_size == 0) segment_size = SEG_DEFAULT_SIZE;

    NexSegcache *sc = (NexSegcache *)calloc(1, sizeof(NexSegcache));
    if (!sc) return NULL;

    sc->max_memory = max_memory;
    sc->n_segments = (uint32_t)(max_memory / segment_size);
    if (sc->n_segments == 0) sc->n_segments = 16;
    if (sc->n_segments > SEG_MAX_SEGMENTS) sc->n_segments = SEG_MAX_SEGMENTS;

    /* Pre-alloca tutti i segmenti */
    sc->segments = (NexSegment *)calloc(sc->n_segments, sizeof(NexSegment));
    if (!sc->segments) {
        free(sc);
        return NULL;
    }

    /* Alloca buffer dati per ogni segmento */
    for (uint32_t i = 0; i < sc->n_segments; i++) {
        sc->segments[i].data = (uint8_t *)calloc(1, segment_size);
        if (!sc->segments[i].data) {
            for (uint32_t j = 0; j < i; j++) free(sc->segments[j].data);
            free(sc->segments);
            free(sc);
            return NULL;
        }
        sc->segments[i].data_size = segment_size;
    }

    /* Stack dei segmenti liberi */
    sc->free_list = (uint32_t *)malloc(sc->n_segments * sizeof(uint32_t));
    if (!sc->free_list) {
        for (uint32_t i = 0; i < sc->n_segments; i++) free(sc->segments[i].data);
        free(sc->segments);
        free(sc);
        return NULL;
    }
    for (uint32_t i = 0; i < sc->n_segments; i++) sc->free_list[i] = i;
    sc->n_free = sc->n_segments;

    /* TTL bucket array */
    sc->ttl_buckets = (NexSegment **)calloc(SEG_TTL_BUCKETS, sizeof(NexSegment *));

    /* Hash table bulk-chaining */
    sc->hash_table = (SegHashBucket *)calloc(SEG_HASH_BUCKETS, sizeof(SegHashBucket));
    sc->hash_mask = SEG_HASH_BUCKETS - 1;

    pthread_mutex_init(&sc->lock, NULL);
    sc->running = 1;

    fprintf(stderr,
            "[NexCache Segcache] Init: %u segments × %uKB = %zuMB\n"
            "  Item overhead: 8 bytes (vs 64 Redis, 56 Memcached)\n"
            "  Expiration: O(1) per segment\n",
            sc->n_segments, segment_size / 1024, max_memory / 1024 / 1024);
    return sc;
}

/* ── Preleva un segmento dal pool libero ─────────────────────── */
static NexSegment *seg_alloc(NexSegcache *sc, uint32_t ttl_bucket) {
    if (sc->n_free == 0) return NULL;
    uint32_t idx = sc->free_list[--sc->n_free];
    NexSegment *seg = &sc->segments[idx];
    memset(seg->data, 0, seg->data_size);
    seg->write_offset = 0;
    seg->n_items = 0;
    seg->ttl_bucket = ttl_bucket;
    seg->create_time = seg_unix_now();
    seg->next = seg->prev = NULL;
    return seg;
}

/* ── Hash table: inserimento ─────────────────────────────────── */
static int ht_insert(NexSegcache *sc, uint64_t hash, uint16_t key_fp, uint32_t seg_idx, uint32_t item_offset) {
    uint32_t bucket_idx = (uint32_t)(hash & sc->hash_mask);
    SegHashBucket *bkt = &sc->hash_table[bucket_idx];

    for (int s = 0; s < SEG_ITEMS_PER_BUCKET; s++) {
        if (!((bkt->occupancy >> s) & 1)) {
            bkt->entries[s].key_fp = key_fp;
            bkt->entries[s].seg_idx = (uint16_t)seg_idx;
            bkt->entries[s].item_offset = item_offset;
            bkt->occupancy |= (1u << s);
            return 0;
        }
    }
    return -1; /* Bucket pieno */
}

/* ── Hash table: ricerca ─────────────────────────────────────── */
static SegHashEntry *ht_find(NexSegcache *sc, uint64_t hash, uint16_t key_fp, const char *key, uint16_t key_len) {
    uint32_t bucket_idx = (uint32_t)(hash & sc->hash_mask);
    SegHashBucket *bkt = &sc->hash_table[bucket_idx];

    for (int s = 0; s < SEG_ITEMS_PER_BUCKET; s++) {
        if (!((bkt->occupancy >> s) & 1)) continue;
        if (bkt->entries[s].key_fp != key_fp) continue;

        /* Verifica chiave nel segmento */
        NexSegment *seg = &sc->segments[bkt->entries[s].seg_idx];
        uint8_t *item_ptr = seg->data + bkt->entries[s].item_offset;
        SegItemHeader hdr;
        memcpy(&hdr, item_ptr, sizeof(hdr));

        if (SEG_HDR_KEY_LEN(hdr) == key_len) {
            const char *stored_key = (const char *)(item_ptr + sizeof(hdr));
            if (memcmp(stored_key, key, key_len) == 0) {
                /* Verifica TTL: calc expire time dal segmento */
                uint32_t ttl_b = seg->ttl_bucket;
                uint32_t ttl_s = (uint32_t)(ttl_b < 60 ? ttl_b : ttl_b < 114 ? (ttl_b - 60) * 10 + 60
                                                                             : (ttl_b - 114) * 60 + 600);
                uint32_t expire = seg->create_time + ttl_s;
                if (ttl_s > 0 && expire <= seg_unix_now()) {
                    /* Scaduto: rimuovi dalla hash table */
                    bkt->occupancy &= ~(1u << s);
                    return NULL;
                }
                return &bkt->entries[s];
            }
        }
    }
    return NULL;
}

/* ── segcache_set ────────────────────────────────────────────── */
int segcache_set(NexSegcache *sc, const char *key, uint16_t key_len, const uint8_t *value, uint16_t value_len, uint32_t ttl_seconds) {
    if (!sc || !key || !value) return -1;

    uint32_t item_size = (uint32_t)(sizeof(SegItemHeader) + key_len + value_len);

    pthread_mutex_lock(&sc->lock);

    uint32_t ttl_b = ttl_to_bucket(ttl_seconds);

    /* Trova/crea segmento per questo TTL bucket */
    NexSegment *seg = sc->ttl_buckets[ttl_b];
    if (!seg || seg->write_offset + item_size > seg->data_size) {
        seg = seg_alloc(sc, ttl_b);
        if (!seg) {
            pthread_mutex_unlock(&sc->lock);
            return -1; /* Out of segments */
        }
        seg->next = sc->ttl_buckets[ttl_b];
        if (sc->ttl_buckets[ttl_b])
            sc->ttl_buckets[ttl_b]->prev = seg;
        sc->ttl_buckets[ttl_b] = seg;
    }

    /* Calcola indice del segmento */
    uint32_t seg_idx = (uint32_t)(seg - sc->segments);
    uint32_t item_offset = seg->write_offset;

    /* Scrivi header + key + value nel buffer del segmento */
    uint64_t hash64 = seg_hash(key, key_len);
    uint16_t key_fp = (uint16_t)(hash64 >> 48);

    SegItemHeader hdr = 0;
    hdr |= ((uint64_t)key_fp << 48);
    hdr |= ((uint64_t)key_len << 32);
    hdr |= ((uint64_t)value_len << 16);
    hdr |= (uint64_t)ttl_b;

    uint8_t *dst = seg->data + seg->write_offset;
    memcpy(dst, &hdr, sizeof(hdr));
    memcpy(dst + sizeof(hdr), key, key_len);
    memcpy(dst + sizeof(hdr) + key_len, value, value_len);

    seg->write_offset += item_size;
    seg->n_items++;

    /* Inserisci in hash table */
    ht_insert(sc, hash64, key_fp, seg_idx, item_offset);

    sc->used_memory += item_size;
    sc->stats.sets++;
    sc->stats.bytes_stored += item_size;

    pthread_mutex_unlock(&sc->lock);
    return 0;
}

/* ── segcache_get ────────────────────────────────────────────── */
int segcache_get(NexSegcache *sc, const char *key, uint16_t key_len, uint8_t *value_buf, uint16_t buf_len, uint16_t *value_len_out) {
    if (!sc || !key || !value_buf) return -1;

    uint64_t hash64 = seg_hash(key, key_len);
    uint16_t key_fp = (uint16_t)(hash64 >> 48);

    pthread_mutex_lock(&sc->lock);
    SegHashEntry *entry = ht_find(sc, hash64, key_fp, key, key_len);
    if (!entry) {
        sc->stats.misses++;
        sc->stats.gets++;
        sc->stats.hit_rate = (double)sc->stats.hits /
                             (double)(sc->stats.hits + sc->stats.misses);
        pthread_mutex_unlock(&sc->lock);
        return -1;
    }

    NexSegment *seg = &sc->segments[entry->seg_idx];
    uint8_t *item_ptr = seg->data + entry->item_offset;
    SegItemHeader hdr;
    memcpy(&hdr, item_ptr, sizeof(hdr));

    uint16_t vlen = SEG_HDR_VAL_LEN(hdr);
    if (vlen > buf_len) {
        if (value_len_out) *value_len_out = vlen;
        pthread_mutex_unlock(&sc->lock);
        return -2;
    }

    memcpy(value_buf, item_ptr + sizeof(hdr) + SEG_HDR_KEY_LEN(hdr), vlen);
    if (value_len_out) *value_len_out = vlen;

    sc->stats.hits++;
    sc->stats.gets++;
    sc->stats.hit_rate = (double)sc->stats.hits /
                         (double)(sc->stats.hits + sc->stats.misses);
    pthread_mutex_unlock(&sc->lock);
    return 0;
}

/* ── segcache_del ──────────────────────────────────────────────── */
int segcache_del(NexSegcache *sc, const char *key, uint16_t key_len) {
    if (!sc || !key) return -1;
    uint64_t hash64 = seg_hash(key, key_len);
    uint16_t key_fp = (uint16_t)(hash64 >> 48);

    pthread_mutex_lock(&sc->lock);
    uint32_t bucket_idx = (uint32_t)(hash64 & sc->hash_mask);
    SegHashBucket *bkt = &sc->hash_table[bucket_idx];

    for (int s = 0; s < SEG_ITEMS_PER_BUCKET; s++) {
        if (!((bkt->occupancy >> s) & 1)) continue;
        if (bkt->entries[s].key_fp != key_fp) continue;
        /* Trovato: rimuovi dall'hash table */
        bkt->occupancy &= ~(1u << s);
        sc->stats.dels++;
        pthread_mutex_unlock(&sc->lock);
        return 1;
    }
    pthread_mutex_unlock(&sc->lock);
    return 0;
}

int segcache_exists(NexSegcache *sc, const char *key, uint16_t key_len) {
    if (!sc || !key) return 0;
    uint64_t hash64 = seg_hash(key, key_len);
    uint16_t key_fp = (uint16_t)(hash64 >> 48);

    pthread_mutex_lock(&sc->lock);
    uint32_t bucket_idx = (uint32_t)(hash64 & sc->hash_mask);
    SegHashBucket *bkt = &sc->hash_table[bucket_idx];

    for (int s = 0; s < SEG_ITEMS_PER_BUCKET; s++) {
        if (!((bkt->occupancy >> s) & 1)) continue;
        if (bkt->entries[s].key_fp != key_fp) continue;

        /* Verifica chiave */
        NexSegment *seg = &sc->segments[bkt->entries[s].seg_idx];
        uint8_t *item_ptr = seg->data + bkt->entries[s].item_offset;
        SegItemHeader hdr;
        memcpy(&hdr, item_ptr, sizeof(hdr));

        if (SEG_HDR_KEY_LEN(hdr) == key_len) {
            const char *stored_key = (const char *)(item_ptr + sizeof(hdr));
            if (memcmp(stored_key, key, key_len) == 0) {
                /* Verifica TTL */
                uint32_t ttl_b = seg->ttl_bucket;
                uint32_t ttl_s = (uint32_t)(ttl_b < 60 ? ttl_b : ttl_b < 114 ? (ttl_b - 60) * 10 + 60
                                                                             : (ttl_b - 114) * 60 + 600);
                if (ttl_s > 0) {
                    uint32_t expire = seg->create_time + ttl_s;
                    if (expire <= (uint32_t)time(NULL)) {
                        bkt->occupancy &= ~(1u << s);
                        pthread_mutex_unlock(&sc->lock);
                        return 0;
                    }
                }
                pthread_mutex_unlock(&sc->lock);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&sc->lock);
    return 0;
}

/* ── Expiration O(1): libera un intero segmento scaduto ─────── */
uint32_t segcache_expire_segments(NexSegcache *sc) {
    if (!sc) return 0;
    uint32_t expired = 0;
    uint32_t now = seg_unix_now();

    pthread_mutex_lock(&sc->lock);
    for (uint32_t b = 0; b < SEG_TTL_BUCKETS; b++) {
        NexSegment *seg = sc->ttl_buckets[b];
        while (seg) {
            /* TTL approssimativo del bucket */
            uint32_t ttl_s = (uint32_t)(b < 60 ? b : b < 114 ? (b - 60) * 10 + 60
                                                             : (b - 114) * 60 + 600);
            uint32_t expire_time = seg->create_time + ttl_s;

            if (ttl_s > 0 && expire_time <= now) {
                /* Tutto il segmento è scaduto: libera O(1) */
                NexSegment *next = seg->next;
                if (seg->prev)
                    seg->prev->next = seg->next;
                else
                    sc->ttl_buckets[b] = seg->next;
                if (seg->next) seg->next->prev = seg->prev;

                /* Rimetti nel pool libero */
                uint32_t seg_idx = (uint32_t)(seg - sc->segments);
                sc->free_list[sc->n_free++] = seg_idx;
                sc->stats.evictions_segment++;
                sc->stats.expirations += seg->n_items;
                expired += seg->n_items;
                seg = next;
            } else {
                seg = seg->next;
            }
        }
    }
    pthread_mutex_unlock(&sc->lock);
    return expired;
}

SegStats segcache_get_stats(NexSegcache *sc) {
    SegStats empty = {0};
    if (!sc) return empty;
    pthread_mutex_lock(&sc->lock);
    SegStats s = sc->stats;
    s.active_segments = sc->n_segments - sc->n_free;
    pthread_mutex_unlock(&sc->lock);
    return s;
}

void segcache_print_stats(NexSegcache *sc) {
    if (!sc) return;
    SegStats s = segcache_get_stats(sc);
    fprintf(stderr,
            "[NexCache Segcache] Stats:\n"
            "  gets=%llu hits=%llu miss=%llu rate=%.2f%%\n"
            "  sets=%llu dels=%llu\n"
            "  evict_seg=%llu expire=%llu\n"
            "  active_segs=%u free=%u\n"
            "  bytes_stored=%llu\n",
            (unsigned long long)s.gets,
            (unsigned long long)s.hits,
            (unsigned long long)s.misses,
            s.hit_rate * 100.0,
            (unsigned long long)s.sets,
            (unsigned long long)s.dels,
            (unsigned long long)s.evictions_segment,
            (unsigned long long)s.expirations,
            s.active_segments, sc->n_free,
            (unsigned long long)s.bytes_stored);
}

void segcache_destroy(NexSegcache *sc) {
    if (!sc) return;
    for (uint32_t i = 0; i < sc->n_segments; i++) free(sc->segments[i].data);
    free(sc->segments);
    free(sc->free_list);
    free(sc->ttl_buckets);
    free(sc->hash_table);
    pthread_mutex_destroy(&sc->lock);
    free(sc);
}
