/* NexCache Subkey TTL — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "subkey_ttl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t unix_now(void) {
    return (uint32_t)time(NULL);
}

/* ── subkey_ttl_create ──────────────────────────────────────── */
SubkeyTTL *subkey_ttl_create(const char *parent_key,
                             SubkeyContainerType type) {
    SubkeyTTL *s = (SubkeyTTL *)calloc(1, sizeof(SubkeyTTL));
    if (!s) return NULL;

    s->capacity = 64;
    s->entries = (SubkeyEntry *)calloc(s->capacity, sizeof(SubkeyEntry));
    if (!s->entries) {
        free(s);
        return NULL;
    }
    s->count = 0;
    s->container_type = type;
    if (parent_key)
        strncpy(s->parent_key, parent_key, sizeof(s->parent_key) - 1);
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

/* ── subkey_ttl_set ─────────────────────────────────────────── */
int subkey_ttl_set(SubkeyTTL *s, const uint8_t *field, uint8_t field_len, uint32_t expire_unix) {
    if (!s || !field || field_len == 0) return -1;
    pthread_mutex_lock(&s->lock);

    /* Aggiorna se esiste già */
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->entries[i].field_len == field_len &&
            memcmp(s->entries[i].field, field, field_len) == 0) {
            s->entries[i].expire_unix = expire_unix;
            pthread_mutex_unlock(&s->lock);
            return 1;
        }
    }

    /* Inserisci nuovo */
    if (s->count >= s->capacity) {
        uint32_t ncap = s->capacity * 2;
        SubkeyEntry *ne = (SubkeyEntry *)realloc(s->entries,
                                                 ncap * sizeof(SubkeyEntry));
        if (!ne) {
            pthread_mutex_unlock(&s->lock);
            return -1;
        }
        s->entries = ne;
        s->capacity = ncap;
    }

    SubkeyEntry *e = &s->entries[s->count++];
    memset(e, 0, sizeof(*e));
    uint8_t copy_len = field_len < 255 ? field_len : 255;
    memcpy(e->field, field, copy_len);
    e->field_len = copy_len;
    e->expire_unix = expire_unix;

    pthread_mutex_unlock(&s->lock);
    return 0;
}

/* ── subkey_ttl_clear ───────────────────────────────────────── */
int subkey_ttl_clear(SubkeyTTL *s, const uint8_t *field, uint8_t field_len) {
    if (!s || !field) return -1;
    pthread_mutex_lock(&s->lock);
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->entries[i].field_len == field_len &&
            memcmp(s->entries[i].field, field, field_len) == 0) {
            /* Rimuovi spostando l'ultimo */
            s->entries[i] = s->entries[--s->count];
            pthread_mutex_unlock(&s->lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&s->lock);
    return 0;
}

/* ── subkey_ttl_get ─────────────────────────────────────────── */
uint32_t subkey_ttl_get(SubkeyTTL *s, const uint8_t *field, uint8_t field_len) {
    if (!s || !field) return 0;
    pthread_mutex_lock(&s->lock);
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->entries[i].field_len == field_len &&
            memcmp(s->entries[i].field, field, field_len) == 0) {
            uint32_t e = s->entries[i].expire_unix;
            pthread_mutex_unlock(&s->lock);
            return e;
        }
    }
    pthread_mutex_unlock(&s->lock);
    return 0;
}

/* ── subkey_ttl_expire ──────────────────────────────────────── */
int subkey_ttl_expire(SubkeyTTL *s, SubkeyExpireCb cb, void *ctx) {
    if (!s || !cb) return 0;
    uint32_t now = unix_now();
    uint32_t expired = 0;

    pthread_mutex_lock(&s->lock);
    uint32_t i = 0;
    while (i < s->count) {
        if (s->entries[i].expire_unix > 0 &&
            s->entries[i].expire_unix <= now) {
            /* Chiama callback prima di rimuovere */
            pthread_mutex_unlock(&s->lock);
            cb(s->parent_key,
               s->entries[i].field, s->entries[i].field_len,
               s->container_type, ctx);
            pthread_mutex_lock(&s->lock);
            /* Rimuovi: swappa con l'ultimo */
            s->entries[i] = s->entries[--s->count];
            expired++;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&s->lock);
    return (int)expired;
}

uint32_t subkey_ttl_active_count(SubkeyTTL *s) {
    if (!s) return 0;
    pthread_mutex_lock(&s->lock);
    uint32_t n = 0;
    uint32_t now = unix_now();
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->entries[i].expire_unix == 0 ||
            s->entries[i].expire_unix > now) n++;
    }
    pthread_mutex_unlock(&s->lock);
    return n;
}

void subkey_ttl_destroy(SubkeyTTL *s) {
    if (!s) return;
    pthread_mutex_destroy(&s->lock);
    free(s->entries);
    free(s);
}

/* ── SubkeyTTLManager (background scan) ─────────────────────── */
static void *subkey_mgr_thread(void *arg) {
    SubkeyTTLManager *mgr = (SubkeyTTLManager *)arg;
    while (mgr->running) {
        usleep((unsigned int)mgr->scan_interval_ms * 1000);

        pthread_mutex_lock(&mgr->lock);
        for (uint32_t i = 0; i < mgr->count; i++) {
            if (mgr->tables[i]) {
                int n = subkey_ttl_expire(mgr->tables[i],
                                          mgr->global_cb, mgr->global_ctx);
                if (n > 0) mgr->expired_total += (uint64_t)n;
            }
        }
        pthread_mutex_unlock(&mgr->lock);
    }
    return NULL;
}

SubkeyTTLManager *subkey_mgr_create(SubkeyExpireCb cb, void *ctx) {
    SubkeyTTLManager *mgr = (SubkeyTTLManager *)calloc(1, sizeof(SubkeyTTLManager));
    if (!mgr) return NULL;
    mgr->capacity = 256;
    mgr->tables = (SubkeyTTL **)calloc(mgr->capacity, sizeof(SubkeyTTL *));
    mgr->global_cb = cb;
    mgr->global_ctx = ctx;
    mgr->scan_interval_ms = 1000;
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->running = 1;
    pthread_create(&mgr->thread, NULL, subkey_mgr_thread, mgr);
    fprintf(stderr,
            "[NexCache SubkeyTTL] Manager started (scan_interval=%ums)\n"
            "  Supports Hash/Set/ZSet/List/JSON/Vector TTL on sub-elements.\n",
            mgr->scan_interval_ms);
    return mgr;
}

int subkey_mgr_register(SubkeyTTLManager *mgr, SubkeyTTL *s) {
    if (!mgr || !s) return -1;
    pthread_mutex_lock(&mgr->lock);
    if (mgr->count >= mgr->capacity) {
        uint32_t nc = mgr->capacity * 2;
        SubkeyTTL **nt = (SubkeyTTL **)realloc(mgr->tables,
                                               nc * sizeof(SubkeyTTL *));
        if (!nt) {
            pthread_mutex_unlock(&mgr->lock);
            return -1;
        }
        mgr->tables = nt;
        mgr->capacity = nc;
    }
    mgr->tables[mgr->count++] = s;
    pthread_mutex_unlock(&mgr->lock);
    return 0;
}

int subkey_mgr_unregister(SubkeyTTLManager *mgr, SubkeyTTL *s) {
    if (!mgr || !s) return -1;
    pthread_mutex_lock(&mgr->lock);
    for (uint32_t i = 0; i < mgr->count; i++) {
        if (mgr->tables[i] == s) {
            mgr->tables[i] = mgr->tables[--mgr->count];
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&mgr->lock);
    return -1;
}

void subkey_mgr_print_stats(SubkeyTTLManager *mgr) {
    if (!mgr) return;
    fprintf(stderr,
            "[NexCache SubkeyTTL] tables=%u expired_total=%llu\n",
            mgr->count,
            (unsigned long long)mgr->expired_total);
}

void subkey_mgr_destroy(SubkeyTTLManager *mgr) {
    if (!mgr) return;
    mgr->running = 0;
    pthread_join(mgr->thread, NULL);
    pthread_mutex_destroy(&mgr->lock);
    free(mgr->tables);
    free(mgr);
}
