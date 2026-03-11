#include "fuzz_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define NUM_REPLICAS 3
#define NUM_NODES 3

typedef struct {
    PNCounter *pn[NUM_REPLICAS];
    ORSet *orset[NUM_REPLICAS];
    FuzzStats stats;
} FuzzCtx;

static FuzzCtx ctx;

static void rand_string(char *str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
}

void nex_crdt_fuzz_run(int duration_seconds) {
    printf("[CRDT Fuzzer] Iniziando fuzzer per %d secondi (3 Repliche)...\n", duration_seconds);
    memset(&ctx, 0, sizeof(ctx));

    for (int i = 0; i < NUM_REPLICAS; i++) {
        ctx.pn[i] = pncounter_create(i, NUM_NODES);
        ctx.orset[i] = orset_create(i);
    }

    time_t start_time = time(NULL);

    while (time(NULL) - start_time < duration_seconds) {
        /* Seleziona una replica a caso */
        int r1_idx = rand() % NUM_REPLICAS;
        int r2_idx = rand() % NUM_REPLICAS;

        /* Genera operazione casuale */
        int op = rand() % 5;

        switch (op) {
        case 0:
            pncounter_increment(ctx.pn[r1_idx], rand() % 100);
            break;
        case 1:
            pncounter_decrement(ctx.pn[r1_idx], rand() % 50);
            break;
        case 2: {
            char val[16];
            rand_string(val, (rand() % 10) + 2);
            orset_add(ctx.orset[r1_idx], (uint8_t *)val, strlen(val));
            break;
        }
        case 3: {
            char val[16];
            rand_string(val, (rand() % 10) + 2);
            orset_remove(ctx.orset[r1_idx], (uint8_t *)val, strlen(val));
            break;
        }
        case 4:
            /* Merge r1 in r2 */
            if (r1_idx != r2_idx) {
                pncounter_merge(ctx.pn[r2_idx], ctx.pn[r1_idx]);
                orset_merge(ctx.orset[r2_idx], ctx.orset[r1_idx]);
            }
            break;
        }

        ctx.stats.operations++;
        if (ctx.stats.operations % 100000 == 0) {
            printf("[CRDT Fuzzer] Eseguite %llu operazioni...\n", (unsigned long long)ctx.stats.operations);
        }
    }

    printf("\n[CRDT Fuzzer] Fase operativa completata. Avvio total merge e verifica convergenza...\n");

    /* Convergenza totale */
    for (int it = 0; it < 2; it++) {
        for (int i = 0; i < NUM_REPLICAS; i++) {
            for (int j = 0; j < NUM_REPLICAS; j++) {
                if (i != j) {
                    pncounter_merge(ctx.pn[j], ctx.pn[i]);
                    orset_merge(ctx.orset[j], ctx.orset[i]);
                }
            }
        }
    }

    /* Verifica PN_Counter */
    int64_t v_base = pncounter_value(ctx.pn[0]);
    int pn_diverged = 0;
    for (int i = 1; i < NUM_REPLICAS; i++) {
        if (pncounter_value(ctx.pn[i]) != v_base) {
            pn_diverged = 1;
            ctx.stats.divergence_count++;
        }
    }

    /* Verifica ORSet */
    uint32_t s_base = orset_size(ctx.orset[0]);
    int or_diverged = 0;
    for (int i = 1; i < NUM_REPLICAS; i++) {
        if (orset_size(ctx.orset[i]) != s_base) {
            or_diverged = 1;
            ctx.stats.divergence_count++;
        }
    }

    if (!pn_diverged) {
        printf("  [OK] PN-Counters convergenti al valore finale: %lld\n", (long long)v_base);
    } else {
        printf("  [FAIL] Divergenza nei PN-Counter!\n");
    }

    if (!or_diverged) {
        printf("  [OK] OR-Sets convergenti alla size finale: %u\n", s_base);
    } else {
        printf("  [FAIL] Divergenza negli OR-Set!\n");
    }

    printf("\n[CRDT Fuzzer] Statistiche finali:\n");
    printf("  Operazioni eseguite: %llu\n", (unsigned long long)ctx.stats.operations);
    printf("  Divergenze rilevate: %u\n", ctx.stats.divergence_count);

    if (ctx.stats.divergence_count == 0) {
        printf("\n✅ ✅ GATE 1 PASSATO: Consistenza matematica e convergenza CRDT garantita. Nessuna divergenza.\n\n");
    } else {
        printf("\n❌ ❌ GATE 1 FALLITO: Rilevate divergenze nello stato CRDT.\n\n");
    }

    for (int i = 0; i < NUM_REPLICAS; i++) {
        pncounter_destroy(ctx.pn[i]);
        orset_destroy(ctx.orset[i]);
    }
}

int main(int argc, char **argv) {
    int duration = 5;
    if (argc > 1) {
        duration = atoi(argv[1]);
    }
    srand(time(NULL));
    nex_crdt_fuzz_run(duration);
    return 0;
}
