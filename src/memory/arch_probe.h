#ifndef NEXCACHE_ARCH_PROBE_H
#define NEXCACHE_ARCH_PROBE_H

#include <stdint.h>
#include <stdbool.h>

// Struttura singleton — popolata da nexarch_probe() all'avvio
typedef struct NexArchInfo {
    // — Bit disponibili per i tagged pointer metadata —
    int metadata_shift; // bit da cui iniziano i metadata (es. 48 o 56)
    int metadata_bits;  // quanti bit sono sicuri (es. 16 su x86 o 8 su ARM64)
    uint64_t addr_mask; // maschera per estrarre l'indirizzo puro

    // — Feature rilevate —
    bool tbi_enabled;  // ARM64: Top Byte Ignore attivo
    bool pac_enabled;  // ARM64: Pointer Auth Code attivo (riduce bit liberi)
    bool la57_enabled; // x86-64: 5-level paging (57-bit VA)
    bool lam_enabled;  // x86-64: Linear Address Masking (Intel Ice Lake+)

    // — Descrizione leggibile per log di avvio —
    char description[128];
} NexArchInfo;

// Variabile globale (read-only dopo l'init)
extern NexArchInfo g_nexarch;

// Inizializza g_nexarch — deve essere chiamata PRIMA di qualsiasi
// allocazione Arena o creazione NexDashTable.
// Ritorna 0 se OK, -1 se architettura non supportata.
int nexarch_probe(void);

#endif // NEXCACHE_ARCH_PROBE_H
