# NexCache Internal Architecture

Questo documento descrive le scelte ingegneristiche profonde che rendono NexCache il sistema in-memory più efficiente del 2026.

## 1. Gestione della Memoria: Arena vs Heap
I database tradizionali soffrono di **frammentazione esterna**. Quando molte piccole chiavi vengono create e cancellate, il sistema operativo fatica a ricompattare la memoria, portando a un "RSS gap" dove il database usa molta più RAM di quella necessaria per i dati.

**La soluzione NexCache:**
*   **Arena Allocator**: Tutto ciò che è correlato a un'operazione viene allocato in un blocco di memoria contiguo.
*   **Linear Key Pool**: Le chiavi non sono oggetti sparsi, ma stringhe impacchettate in un buffer lineare. Gli indici usano offset a 32 bit invece di puntatori a 64 bit, risparmiando il 50% di spazio solo sugli indici.

## 2. NexDash: La Hash Table di Nuova Generazione
NexDash si basa sulla ricerca accademica sulle "Dash Tables" (Scalable Hashing on Persistent Memory) ma ottimizzata per la RAM DRAM.

*   **Segmented Sharding**: La tabella è divisa in segmenti. Ogni thread worker gestisce i propri segmenti senza lock.
*   **24-Byte Slot**: Ogni slot contiene:
    *   Hash della chiave (8 byte)
    *   Tagged Pointer (8 byte) - contiene l'indirizzo del valore + metadati (tipo, TTL, versione)
    *   Key Offset (4 byte) - posizione nel pool lineare.
    *   Expiry (4 byte) - tempo di scadenza compresso.

## 3. Tagged Pointers e Sicurezza Architetturale
NexCache utilizza i 16 bit meno significativi e i bit superiori non usati dei puntatori a 64 bit per memorizzare metadati "in-place". 
Per garantire che NexCache sia **inattaccabile**, abbiamo implementato un **Dynamic Arch-Probe**. All'avvio, il software interroga il sistema operativo per rilevare:
*   **ARM PAC (Pointer Authentication)**: Presente su chip Apple e AWS Graviton.
*   **Intel LA57/LAM**: Il paging a 5 livelli.

Se rileva queste tecnologie, NexCache sposta automaticamente i suoi metadati nei bit "sicuri", evitando crash che affliggono altri database meno sofisticati.

## 4. Vector AI Engine Routing
Non tutti i vettori sono uguali. NexCache non ti costringe a usare un solo algoritmo.
*   **Small datasets (<100k)**: Usa ricerca Flat/Exact per precisione al 100%.
*   **Dynamic Sets**: Usa **HNSW** (Hierarchical Navigable Small World) ottimizzato per la cache CPU.
*   **Giant Sets (10M+ o CXL)**: Passa automaticamente a **DiskANN/Vamana** per sfruttare la velocità degli SSD NVMe o delle memorie CXL senza saturare la RAM costosa.

## 5. Network Dual-Path
NexCache rileva il tipo di connessione del client:
*   **Standard TCP**: Supporto totale per `redis-cli`, librerie Python, Java, Go esistenti.
*   **Accelerated path**: Se rileva una scheda di rete compatibile e un client enterprise, può attivare comunicazioni via **io_uring** o kernel-bypass per abbattere la latenza sotto i 100μs.
