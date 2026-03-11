/* NexCache CRDT Fuzzer & Validator
 * ============================================================
 * Esegue stress test di consistenza per i moduli CRDT.
 * Verifica che:
 *   - L'ordine delle operazioni non influenzi lo stato finale (Commutatività).
 *   - Operazioni replicate più volte non alterino lo stato (Idempotenza).
 *   - Il merge tra shard diversi converga sempre (Convergenza).
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_CRDT_FUZZ_H
#define NEXCACHE_CRDT_FUZZ_H

#include "crdt.h"

typedef struct FuzzStats {
    uint64_t iterations;
    uint64_t operations;
    uint32_t divergence_count;
} FuzzStats;

/* API di Test */
void nex_crdt_fuzz_run(int duration_seconds);
int nex_crdt_verify_convergence(void *replica_a, void *replica_b);

#endif /* NEXCACHE_CRDT_FUZZ_H */
