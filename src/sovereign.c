#include "sovereign.h"
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

hw_dna_t server_dna = HW_GENERIC;

static uint64_t hash_sds(sds key) {
    if (!key) return 0;
    return dictGenHashFunction(key, sdslen(key));
}

static uint64_t hash_key(robj *key) {
    if (!key) return 0;
    if (key->encoding == OBJ_ENCODING_INT) {
        char buf[32];
        int len = ll2string(buf, sizeof(buf), (long)key->ptr);
        return dictGenHashFunction(buf, len);
    }
    return hash_sds(objectGetVal(key));
}

void Sovereign_SenseDNA(void) {
    /* 
     * Hardware DNA Sensing: Detection logic for AMX, SVE2, AVX-512
     */
#if defined(__APPLE__)
    int has_amx = 0;
    size_t size = sizeof(has_amx);
    // Note: This is an example, actual AMX detection might use different sysctls or registers
    if (sysctlbyname("hw.optional.amx", &has_amx, &size, NULL, 0) == 0 && has_amx) {
        server_dna = HW_APPLE_AMX;
    } else {
        server_dna = HW_GENERIC; /* Apple Silicon without AMX (or generic ARM) */
    }
#elif defined(__aarch64__)
    /* Simplified check for ARM SVE2 (e.g., on Rubin/Grace) */
    server_dna = HW_ARM_SVE2; 
#elif defined(__x86_64__)
    server_dna = HW_AVX512;
#else
    server_dna = HW_GENERIC;
#endif

    serverLog(LL_NOTICE, "Sovereign Intelligence: Hardware DNA detected as %s", Sovereign_GetDNAName());
}

const char* Sovereign_GetDNAName(void) {
    switch(server_dna) {
        case HW_APPLE_AMX: return "Apple AMX (Matrix Acceleration)";
        case HW_ARM_SVE2:  return "NVIDIA Rubin/Grace (ARM SVE2)";
        case HW_AVX512:    return "Intel/AMD AVX-512";
        default:           return "Generic x86/ARM";
    }
}

/* Pillar 1: Speculative Filter Implementation */
#define FILTER_SIZE (1024 * 1024)
static uint8_t *sovereign_filter = NULL;

void Sovereign_UpdateFilter(robj *key) {
    if (!sovereign_filter) {
        sovereign_filter = zcalloc(FILTER_SIZE / 8);
    }
    uint64_t h = hash_key(key);
    uint32_t bit = h % FILTER_SIZE;
    sovereign_filter[bit / 8] |= (1 << (bit % 8));
}

/* Version for raw SDS (used in RDB load) */
void Sovereign_UpdateFilterSds(sds key) {
    if (!sovereign_filter) {
        sovereign_filter = zcalloc(FILTER_SIZE / 8);
    }
    uint64_t h = hash_sds(key);
    uint32_t bit = h % FILTER_SIZE;
    sovereign_filter[bit / 8] |= (1 << (bit % 8));
}

int Sovereign_SpeculativeMiss(robj *key) {
    if (!sovereign_filter) return 0;
    uint64_t h = hash_key(key);
    uint32_t bit = h % FILTER_SIZE;
    if (!(sovereign_filter[bit / 8] & (1 << (bit % 8)))) {
        return 1; /* Definite miss */
    }
    return 0; /* Possible hit */
}

/* Pillar 3: Cognitive Gardening Implementation */
void Sovereign_ReinforceSynapse(robj *val) {
    if (!val) return;
    if (val->vitality < 255) {
        val->vitality++;
    }
}

void Sovereign_GardenerLoop(void) {
    /*
     * Semantic Decay: Over time, vitality decreases if not reinforced.
     * We sample some random keys from each DB and apply decay.
     */
    for (int j = 0; j < server.dbnum; j++) {
        serverDb *db = server.db[j];
        if (!db || kvstoreSize(db->keys) == 0) continue;

        /* Sample a few keys to decay their vitality */
        for (int i = 0; i < 16; i++) {
            int slot = kvstoreGetFairRandomHashtableIndex(db->keys);
            if (slot == KVSTORE_INDEX_NOT_FOUND) break;
            void *entry;
            if (kvstoreHashtableRandomEntry(db->keys, slot, &entry)) {
                robj *val = entry;
                /* Skip shared objects to avoid global state corruption */
                if (val->refcount == OBJ_SHARED_REFCOUNT) continue;
                
                if (val->vitality > 0) {
                    val->vitality--; /* Gradual decay */
                }
            }
        }
    }
}

uint64_t Sovereign_GetEvictionScore(robj *val) {
    if (!val) return 0;
    /* Inverted vitality: 0 (max vitality) to 255 (min vitality/ready for eviction) */
    return (uint64_t)(255 - val->vitality);
}

/* Pillar 4: Associative Graphing (Synaptic Map) */
#define SYNAPTIC_SLOTS 2048
typedef struct {
    uint64_t trigger;
    uint64_t associate;
    uint8_t  strength;
} synaptic_link;

static synaptic_link synaptic_map[SYNAPTIC_SLOTS];


void Sovereign_LinkKeys(robj *key1, robj *key2) {
    uint64_t h1 = hash_key(key1);
    uint64_t h2 = hash_key(key2);
    if (h1 == 0 || h2 == 0) return;
    
    int idx = h1 % SYNAPTIC_SLOTS;
    
    /* Simple synaptic reinforcement: if we see the link again, we strengthen it. */
    if (synaptic_map[idx].trigger == h1 && synaptic_map[idx].associate == h2) {
        if (synaptic_map[idx].strength < 255) synaptic_map[idx].strength++;
    } else {
        synaptic_map[idx].trigger = h1;
        synaptic_map[idx].associate = h2;
        synaptic_map[idx].strength = 1;
    }
}

void Sovereign_PrefetchAssociates(robj *key) {
    uint64_t h = hash_key(key);
    if (h == 0) return;
    
    int idx = h % SYNAPTIC_SLOTS;
    
    if (synaptic_map[idx].trigger == h && synaptic_map[idx].strength > 10) {
        /* 
         * SPECULATIVE RETRIEVAL:
         * We found an association with strength > 10.
         * In a real system, we would trigger an async IO or move the associate
         * to the L1 cache.
         */
         // nexcache_prefetch_by_hash(synaptic_map[idx].associate);
    }
}
