/* NexCache CRDT — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "crdt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static uint64_t crdt_us_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* ══════════════════════════════════════════════════════════════
 * G-COUNTER
 * ════════════════════════════════════════════════════════════ */

GCounter *gcounter_create(uint32_t self_node_id, int num_nodes) {
    if (num_nodes > CRDT_MAX_NODES) num_nodes = CRDT_MAX_NODES;
    GCounter *g = (GCounter *)calloc(1, sizeof(GCounter));
    if (!g) return NULL;
    g->self_node = self_node_id;
    g->num_nodes = num_nodes;
    return g;
}

void gcounter_increment(GCounter *g, uint64_t delta) {
    if (!g || g->self_node >= (uint32_t)g->num_nodes) return;
    g->values[g->self_node] += delta;
}

uint64_t gcounter_value(const GCounter *g) {
    if (!g) return 0;
    uint64_t sum = 0;
    for (int i = 0; i < g->num_nodes; i++) sum += g->values[i];
    return sum;
}

void gcounter_merge(GCounter *dst, const GCounter *src) {
    if (!dst || !src) return;
    int n = dst->num_nodes > src->num_nodes ? dst->num_nodes : src->num_nodes;
    if (n > CRDT_MAX_NODES) n = CRDT_MAX_NODES;
    for (int i = 0; i < n; i++) {
        if (src->values[i] > dst->values[i])
            dst->values[i] = src->values[i];
    }
    if (src->num_nodes > dst->num_nodes) dst->num_nodes = src->num_nodes;
}

void gcounter_destroy(GCounter *g) {
    free(g);
}

/* ══════════════════════════════════════════════════════════════
 * PN-COUNTER
 * ════════════════════════════════════════════════════════════ */

PNCounter *pncounter_create(uint32_t self_node_id, int num_nodes) {
    if (num_nodes > CRDT_MAX_NODES) num_nodes = CRDT_MAX_NODES;
    PNCounter *p = (PNCounter *)calloc(1, sizeof(PNCounter));
    if (!p) return NULL;
    p->self_node = self_node_id;
    p->num_nodes = num_nodes;
    return p;
}

void pncounter_increment(PNCounter *p, uint64_t delta) {
    if (!p || p->self_node >= (uint32_t)p->num_nodes) return;
    p->positive[p->self_node] += delta;
}

void pncounter_decrement(PNCounter *p, uint64_t delta) {
    if (!p || p->self_node >= (uint32_t)p->num_nodes) return;
    p->negative[p->self_node] += delta;
}

int64_t pncounter_value(const PNCounter *p) {
    if (!p) return 0;
    uint64_t pos = 0, neg = 0;
    for (int i = 0; i < p->num_nodes; i++) {
        pos += p->positive[i];
        neg += p->negative[i];
    }
    return (int64_t)pos - (int64_t)neg;
}

void pncounter_merge(PNCounter *dst, const PNCounter *src) {
    if (!dst || !src) return;
    int n = dst->num_nodes > src->num_nodes ? dst->num_nodes : src->num_nodes;
    if (n > CRDT_MAX_NODES) n = CRDT_MAX_NODES;
    for (int i = 0; i < n; i++) {
        if (src->positive[i] > dst->positive[i]) dst->positive[i] = src->positive[i];
        if (src->negative[i] > dst->negative[i]) dst->negative[i] = src->negative[i];
    }
    if (src->num_nodes > dst->num_nodes) dst->num_nodes = src->num_nodes;
}

void pncounter_destroy(PNCounter *p) {
    free(p);
}

/* ══════════════════════════════════════════════════════════════
 * OR-SET (Observed-Remove Set)
 * Semantica: un remove vince su un add SOLO se il remove arriva
 * DOPO e rimuove esattamente i tag visti — add concorrenti
 * su nodi diversi NON vengono rimossi.
 * ════════════════════════════════════════════════════════════ */

ORSet *orset_create(uint32_t self_node_id) {
    ORSet *s = (ORSet *)calloc(1, sizeof(ORSet));
    if (!s) return NULL;
    s->self_node = self_node_id;
    s->capacity = 64;
    s->entries = (ORSetEntry *)calloc(s->capacity, sizeof(ORSetEntry));
    if (!s->entries) {
        free(s);
        return NULL;
    }
    s->next_tag = 1;
    pthread_rwlock_init(&s->lock, NULL);
    return s;
}

int orset_add(ORSet *s, const uint8_t *value, uint16_t len) {
    if (!s || !value || len == 0 || len > 255) return -1;
    pthread_rwlock_wrlock(&s->lock);

    /* Espandi se necessario */
    if (s->count >= s->capacity) {
        uint32_t new_cap = s->capacity * 2;
        ORSetEntry *ne = (ORSetEntry *)realloc(s->entries, new_cap * sizeof(ORSetEntry));
        if (!ne) {
            pthread_rwlock_unlock(&s->lock);
            return -1;
        }
        s->entries = ne;
        s->capacity = new_cap;
    }

    ORSetEntry *e = &s->entries[s->count++];
    memset(e, 0, sizeof(*e));
    memcpy(e->value, value, len);
    e->value_len = len;
    e->add_tag = s->next_tag++;
    e->origin_node = s->self_node;
    e->is_removed = 0;

    pthread_rwlock_unlock(&s->lock);
    return 0;
}

int orset_remove(ORSet *s, const uint8_t *value, uint16_t len) {
    if (!s || !value || len == 0) return -1;
    pthread_rwlock_wrlock(&s->lock);

    int removed = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        ORSetEntry *e = &s->entries[i];
        if (!e->is_removed && e->value_len == len &&
            memcmp(e->value, value, len) == 0) {
            /* OR-Set semantics: rimuovi solo i tag visti localmente.
             * Un add concorrente su altro nodo con nuovo tag sopravvive. */
            e->is_removed = 1;
            removed = 1;
        }
    }
    pthread_rwlock_unlock(&s->lock);
    return removed;
}

int orset_contains(ORSet *s, const uint8_t *value, uint16_t len) {
    if (!s || !value || len == 0) return 0;
    pthread_rwlock_rdlock(&s->lock);
    int found = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        ORSetEntry *e = &s->entries[i];
        if (!e->is_removed && e->value_len == len &&
            memcmp(e->value, value, len) == 0) {
            found = 1;
            break;
        }
    }
    pthread_rwlock_unlock(&s->lock);
    return found;
}

uint32_t orset_size(ORSet *s) {
    if (!s) return 0;
    pthread_rwlock_rdlock(&s->lock);
    uint32_t count = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (!s->entries[i].is_removed) count++;
    }
    pthread_rwlock_unlock(&s->lock);
    return count;
}

void orset_merge(ORSet *dst, const ORSet *src) {
    if (!dst || !src) return;
    pthread_rwlock_wrlock(&dst->lock);

    for (uint32_t i = 0; i < src->count; i++) {
        const ORSetEntry *se = &src->entries[i];

        /* Controlla se il tag è già presente nel dst */
        int found = 0;
        for (uint32_t j = 0; j < dst->count; j++) {
            if (dst->entries[j].add_tag == se->add_tag &&
                dst->entries[j].origin_node == se->origin_node) {
                found = 1;
                /* Propaga la rimozione (tombstone) */
                dst->entries[j].is_removed |= se->is_removed;
                break;
            }
        }
        if (!found) {
            /* Aggiunge il nuovo entry dal nodo remoto, mantenendo is_removed state */
            if (dst->count >= dst->capacity) {
                uint32_t nc = dst->capacity * 2;
                ORSetEntry *ne = (ORSetEntry *)realloc(dst->entries, nc * sizeof(ORSetEntry));
                if (!ne) break;
                dst->entries = ne;
                dst->capacity = nc;
            }
            dst->entries[dst->count++] = *se;
        }
    }
    pthread_rwlock_unlock(&dst->lock);
}

void orset_destroy(ORSet *s) {
    if (!s) return;
    pthread_rwlock_destroy(&s->lock);
    free(s->entries);
    free(s);
}

/* ══════════════════════════════════════════════════════════════
 * LWW-REGISTER (Last-Write-Wins con Vector Clock)
 * ════════════════════════════════════════════════════════════ */

LWWRegister *lww_create(uint32_t self_node_id, int num_nodes) {
    LWWRegister *r = (LWWRegister *)calloc(1, sizeof(LWWRegister));
    if (!r) return NULL;
    r->node_id = self_node_id;
    r->vc.num_nodes = num_nodes < CRDT_MAX_NODES ? num_nodes : CRDT_MAX_NODES;
    pthread_mutex_init(&r->lock, NULL);
    return r;
}

int lww_set(LWWRegister *r, const uint8_t *value, uint16_t len) {
    if (!r || !value || len == 0 || len > 1023) return -1;
    pthread_mutex_lock(&r->lock);

    r->timestamp_us = crdt_us_now();
    if (r->node_id < (uint32_t)r->vc.num_nodes)
        r->vc.clocks[r->node_id]++;

    memcpy(r->value, value, len);
    r->value_len = len;

    pthread_mutex_unlock(&r->lock);
    return 0;
}

uint16_t lww_get(const LWWRegister *r, uint8_t *buf, uint16_t buf_len) {
    if (!r || !buf || buf_len == 0) return 0;
    uint16_t copy_len = r->value_len < buf_len ? r->value_len : buf_len;
    memcpy(buf, r->value, copy_len);
    return copy_len;
}

void lww_merge(LWWRegister *dst, const LWWRegister *src) {
    if (!dst || !src) return;
    pthread_mutex_lock(&dst->lock);

    /* LWW: vince il timestamp più alto; tiebreak → node_id più alto */
    int should_update = 0;
    if (src->timestamp_us > dst->timestamp_us) {
        should_update = 1;
    } else if (src->timestamp_us == dst->timestamp_us &&
               src->node_id > dst->node_id) {
        should_update = 1;
    }

    if (should_update) {
        memcpy(dst->value, src->value, src->value_len);
        dst->value_len = src->value_len;
        dst->timestamp_us = src->timestamp_us;
        dst->node_id = src->node_id;
    }

    /* Merge vector clock: max per componente */
    int n = dst->vc.num_nodes > src->vc.num_nodes ? dst->vc.num_nodes : src->vc.num_nodes;
    if (n > CRDT_MAX_NODES) n = CRDT_MAX_NODES;
    for (int i = 0; i < n; i++) {
        if (src->vc.clocks[i] > dst->vc.clocks[i])
            dst->vc.clocks[i] = src->vc.clocks[i];
    }

    pthread_mutex_unlock(&dst->lock);
}

void lww_destroy(LWWRegister *r) {
    if (!r) return;
    pthread_mutex_destroy(&r->lock);
    free(r);
}
