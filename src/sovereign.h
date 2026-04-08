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

/* Pillar 2: Hardware DNA Sensing */
typedef enum {
    HW_GENERIC,
    HW_AVX512,
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

#endif
