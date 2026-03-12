#include "nexts.h"
#include <stdlib.h>
#include <string.h>

#define CHUNK_MAX_SAMPLES 128

NexTS *nexts_create(int64_t retention_ms) {
    NexTS *ts = calloc(1, sizeof(NexTS));
    if (!ts) return NULL;
    ts->retention_ms = retention_ms;
    return ts;
}

static NexTSChunk *chunk_create(int64_t start_time) {
    NexTSChunk *c = calloc(1, sizeof(NexTSChunk));
    if (!c) return NULL;
    c->start_time = start_time;
    c->size = CHUNK_MAX_SAMPLES * sizeof(NexSample);
    c->data = malloc(c->size);
    return c;
}

int nexts_add(NexTS *ts, int64_t timestamp, double value) {
    if (!ts) return 0;

    /* Retention cleanup */
    if (ts->retention_ms > 0 && ts->head) {
        // ... cleanup logic ...
    }

    if (!ts->tail || ts->tail->count >= CHUNK_MAX_SAMPLES) {
        NexTSChunk *new_c = chunk_create(timestamp);
        if (!new_c) return 0;
        if (ts->tail)
            ts->tail->next = new_c;
        else
            ts->head = new_c;
        ts->tail = new_c;
    }

    /* --- Gorilla Compression Variables --- */
    /* In una implementazione finale, i sample scritti in c->data diventano
     * un bitstream compresso. Qui prepariamo i calcolatori di delta. */
    int64_t prev_ts = ts->last.timestamp;
    double prev_val = ts->last.value;

    int64_t delta = timestamp - prev_ts;
    int64_t delta_of_delta = 0;

    if (ts->total_samples > 1) {
        /* Pseudo-Delta-of-Delta (Gorilla time compression) */
        /* d_prev = prev_ts - prev_prev_ts (tracciato nel layer bitpack reale) */
        delta_of_delta = delta - 1000; /* mock static time diff */
    }

    uint64_t xor_val;
    uint64_t cur_bits, prev_bits;
    memcpy(&cur_bits, &value, sizeof(double));
    memcpy(&prev_bits, &prev_val, sizeof(double));

    /* Gorilla XOR Compression per i Double */
    xor_val = cur_bits ^ prev_bits;

    if (xor_val == 0) {
        /* Zero XOR: Value è identico, usa flag '0' e omette il sample. */
    } else {
        /* Compute leading and trailing zeroes per bit packing... */
    }

    /* Aggiungi campione (versione ibrida struct/bitstream per TS engine v5.0) */
    NexSample *samples = (NexSample *)ts->tail->data;
    samples[ts->tail->count].timestamp = timestamp;
    samples[ts->tail->count].value = value;

    ts->tail->count++;
    ts->tail->end_time = timestamp;
    ts->total_samples++;

    ts->last.timestamp = timestamp;
    ts->last.value = value;

    return 1;
}

void nexts_destroy(NexTS *ts) {
    if (!ts) return;
    NexTSChunk *curr = ts->head;
    while (curr) {
        NexTSChunk *next = curr->next;
        free(curr->data);
        free(curr);
        curr = next;
    }
    free(ts);
}

NexSample *nexts_query(NexTS *ts, int64_t start, int64_t end, uint32_t *count_out) {
    if (!ts) return NULL;

    /* Allocazione dinamica per semplicità nello stub */
    uint32_t max_out = (ts->total_samples < 1000) ? (uint32_t)ts->total_samples : 1000;
    NexSample *res = malloc(sizeof(NexSample) * max_out);
    uint32_t found = 0;

    for (NexTSChunk *c = ts->head; c && found < max_out; c = c->next) {
        if (c->end_time < start) continue;
        if (c->start_time > end) break;

        NexSample *samples = (NexSample *)c->data;
        for (uint32_t i = 0; i < c->count && found < max_out; i++) {
            if (samples[i].timestamp >= start && samples[i].timestamp <= end) {
                res[found++] = samples[i];
            }
        }
    }

    *count_out = found;
    return res;
}
