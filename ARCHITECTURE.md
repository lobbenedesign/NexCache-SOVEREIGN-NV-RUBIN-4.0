# NexCache Internal Architecture

Questo documento descrive le scelte ingegneristiche profonde che rendono NexCache il sistema in-memory più efficiente e architetturalmente solido del settore.

## 1. Gestione della Memoria: Arena vs Heap

I database tradizionali (come Redis) soffrono di **frammentazione esterna** e usano allocatori come jemalloc. Quando molte piccole chiavi vengono create e cancellate, si genera un "RSS gap" enorme.

**La soluzione NexCache:**
*   **Arena Allocator**: Operazioni isolate in blocchi contigui.
*   **Linear Key Pool**: Indexing puro tramite offset a 32 bit invece di puntatori a 64 bit per le chiavi nel database, risparmiando il 50% di spazio.
*   **NexSegcache**: Architettura di storage studiata dal framework Pelikan per i workload TTL-heavy, abbattendo l'overhead a soli ~8 byte per entry (contro i classici 64).

## 2. NexDash: La Hash Table di Nuova Generazione

NexDash si ispira a ricerche accademiciche come Scalable Hashing on Persistent Memory, ma è riscritta ed evoluta per DRAM:
*   **Segmented Sharding**: Hash table lock-free per singolo thread worker.
*   **24-Byte Slot**: Ogni slot è iper-compatto:
    * Hash completo (8 byte)
    * Tagged Pointer (8 byte) contenente indirizzo, tipo, TTL-attivo e versione.
    * Key Offset (4 byte) per l'arena lineare.
    * TTL Bucket + Flags (2 byte).
*   **Fork-less Delta Snapshot**: A differenza dei full-snapshot tradizionali o di soluzioni come Dragonfly, grazie al version counter implementato nativamente, NexCache salva su disco o invia in replica solo ciò che è mutato.
*   **Blocked Bloom Filter Interno**: Un filtro Bloom allocato per blocchi da 512 bit (una cache line L1), che con 1 solo cache miss garantisce una risposta immediata a ricerche di chiavi non presenti (impiegando 9.6 bit/entry contro i filtri base Cuckoo/Bloom standard che non ottimizzano le cache line locali).

## 3. Tagged Pointers: Rilevamento Architetturale Dinamico

I database tradizionali sprecano spazio nei metadati. NexCache "nasconde" il tipo, il tier termico e i flag *dentro* il puntatore a 64 bit (nei bit non utilizzati dell'indirizzo virtuale). Ma a differenza delle classiche implementazioni "hard-coded" (che crashano su CPU moderne), NexCache usa il modulo `NexArchProbe`.

All'avvio, in zero secondi e prima di qualsiasi allocazione, interroga la CPU/OS rilevando:
*   **x86-64 LAM (Linear Address Masking) e LA57 (5-level paging)**.
*   **ARM64 PAC (Pointer Authentication) e TBI (Top Byte Ignore)** (es. Graviton3 vs M1).

NexCache adatta lo *shift* dei Tagged Pointers (es. usando rigorosamente solo [63:56] su ARM o [63:57] su Intel LA57), rendendolo l'unico in-memory data store con Tagged Pointers universali e sicuri.

## 4. Vector AI Engine: Auto-Routing

A differenza delle soluzioni vector isolate o plug-in base (come l'HNSW only), NexCache implementa internamente il sub-routing:
*   **Small datasets (<100k)**: Ricerca Flat/Exact per il 100% di precisione.
*   **Dynamic Sets**: HNSW vettorizzato e quantizzato INT8/Binary usando SIMD (AVX-512 o NEON rilevati dinamicamente).
*   **Giant Sets (10M+) / Storage-Tiered**: Transizione su algoritmi studiati per il retrieval a disco/memorie CXL come **DiskANN/Vamana**.

## 5. Network Dual-Path: Epoll e RDMA Built-in

*   **TCP Epoll Standard**: Totalmente compatibile con `redis-cli`, Jedis, node-redis (invariati).
*   **RDMA Core-Builtin**: A differenza di fork che lo propongono come modulo sperimentale, l'engine RDMA in NexCache è parte del Core. Questo significa transizione fluida, fallback obbligatorio integrato, attivazione TLS 1.3 anche su protocollo RDMA, e Replicazione attiva su stack accelerato (latenza ~12µs).
