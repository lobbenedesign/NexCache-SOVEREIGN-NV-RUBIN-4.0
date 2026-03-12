#include "nexbloom.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Hashing: usiamo una variante leggera per selezionare blocco e bit */
static inline uint32_t get_block_index(uint64_t hash, uint32_t num_blocks) {
    return (uint32_t)(hash >> 32) % num_blocks;
}

/* All'interno del blocco da 512 bit, usiamo 8 bit per il lookup (k=8) */
/* Usiamo un trucco: dividiamo il blocco in 8 word da 64 bit */
static inline void set_bit_in_block(uint8_t *block, uint64_t hash) {
    uint32_t h1 = (uint32_t)hash;
    uint32_t h2 = (uint32_t)(hash >> 32);

    for (int i = 0; i < 8; i++) {
        /* Calcoliamo un bit index [0-63] per ogni lane da 64-bit */
        uint32_t lane_hash = h1 + i * h2;
        uint32_t bit_idx = lane_hash % 64;
        ((uint64_t *)block)[i] |= (1ULL << bit_idx);
    }
}

static inline int check_bit_in_block(uint8_t *block, uint64_t hash) {
    uint32_t h1 = (uint32_t)hash;
    uint32_t h2 = (uint32_t)(hash >> 32);

    for (int i = 0; i < 8; i++) {
        uint32_t lane_hash = h1 + i * h2;
        uint32_t bit_idx = lane_hash % 64;
        if (!(((uint64_t *)block)[i] & (1ULL << bit_idx))) {
            return 0; /* Certamente assente */
        }
    }
    return 1; /* Forse presente */
}

NexBloom *nexbloom_create(size_t expected_items, double target_fpr) {
    if (expected_items == 0) expected_items = 1000;
    if (target_fpr <= 0) target_fpr = 0.01;

    /* m = -(n * ln(p)) / (ln(2)^2) */
    double m = -(double)expected_items * log(target_fpr) / (log(2.0) * log(2.0));

    /* Arrotondiamo a multipli di 512 bit (64 byte) */
    uint32_t num_blocks = (uint32_t)ceil(m / 512.0);
    if (num_blocks == 0) num_blocks = 1;

    NexBloom *nb = malloc(sizeof(NexBloom));
    if (!nb) return NULL;

    /* Allineamento a 64 byte per performance ottimali */
    if (posix_memalign((void **)&nb->data, 64, num_blocks * 64) != 0) {
        free(nb);
        return NULL;
    }

    memset(nb->data, 0, num_blocks * 64);
    nb->num_blocks = num_blocks;
    nb->seed = 0x1337BEEF;

    return nb;
}

void nexbloom_destroy(NexBloom *nb) {
    if (!nb) return;
    free(nb->data);
    free(nb);
}

void nexbloom_add(NexBloom *nb, uint64_t hash) {
    if (!nb) return;
    uint32_t bidx = get_block_index(hash, nb->num_blocks);
    set_bit_in_block(nb->data + (bidx * 64), hash);
}

int nexbloom_check(NexBloom *nb, uint64_t hash) {
    if (!nb) return 1; /* Failsafe: se non c'è il bloom, diciamo "forse sì" */
    uint32_t bidx = get_block_index(hash, nb->num_blocks);
    return check_bit_in_block(nb->data + (bidx * 64), hash);
}

void nexbloom_reset(NexBloom *nb) {
    if (!nb) return;
    memset(nb->data, 0, nb->num_blocks * 64);
}

size_t nexbloom_memory_usage(NexBloom *nb) {
    if (!nb) return 0;
    return nb->num_blocks * 64;
}
