/*
 * NexCache-SOVEREIGN Extensions
 * Inspired by NeuralVault "Sovereign Intelligence"
 */

#ifndef __SOVEREIGN_H
#define __SOVEREIGN_H

#include "server.h"

/* Pillar 1: Speculative Metadata Filtering */
typedef struct {
    uint64_t signature;
    uint32_t frequency;
    uint16_t semantic_type;
    uint8_t  vitality;
} sovereignMetadata;

void Sovereign_UpdateFilter(robj *key);
void Sovereign_UpdateFilterSds(sds key);
int Sovereign_SpeculativeMiss(robj *key);

/* Hot-path variants: the caller has already computed the key hash once
 * (e.g. via Sovereign_HashKey()) and passes it in, instead of every pillar
 * re-hashing the same key independently. See lookupKey() in db.c, which used
 * to call Sovereign_SpeculativeMiss() and Sovereign_PrefetchAssociates() back
 * to back -- each hashing the identical key from scratch -- on every single
 * GET/SET. That redundant hashing was cheap in isolation but ran on every
 * command, so it only showed up as a throughput regression under pipelining
 * (CPU-bound), not at P=1 (RTT-bound). */
uint64_t Sovereign_HashKey(robj *key);
int Sovereign_SpeculativeMissByHash(uint64_t h);

/* Pillar 2: Hardware DNA Sensing */
typedef enum {
    HW_GENERIC,
    HW_INTEL_AVX2,
    HW_INTEL_AVX512,
    HW_ARM_SVE2,
    HW_APPLE_AMX
} hw_dna_t;

extern hw_dna_t server_dna;

void Sovereign_Init(void);
void Sovereign_SenseDNA(void);
const char* Sovereign_GetDNAName(void);

/* Pillar 3: Cognitive Memory Gardening */
void Sovereign_GardenerLoop(void);
void Sovereign_ReinforceSynapse(robj *val);
uint64_t Sovereign_GetEvictionScore(robj *val);

/* Pillar 4: Associative Graphing */
void Sovereign_LinkKeys(robj *key1, robj *key2);
void Sovereign_PrefetchAssociates(robj *key);
void Sovereign_PrefetchAssociatesByHash(uint64_t h);

#endif
