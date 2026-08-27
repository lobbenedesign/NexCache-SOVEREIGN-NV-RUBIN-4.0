# NexCache-SOVEREIGN NV-RUBIN (VERAM 4.0)

[Italiano](#italiano) | [English](#english)

---

<a name="italiano"></a>
## 🇮🇹 Versione Italiana

### Visione e Obiettivo del Progetto
**NexCache-SOVEREIGN (VERAM 4.0)** è un fork di Redis in C che sperimenta un layer di ottimizzazioni adattive pensato per l'architettura **NVIDIA Rubin-class** (rilevamento hardware a boot-time, filtro speculativo dei miss, eviction "biologica" invece di LRU/LFU puro). È il seguito diretto di [nexcache-VERAM3.3](https://github.com/lobbenedesign/nexcache-VERAM3.3), con gli stessi 4 pilastri concettuali riportati qui sotto in un design più recente.

**Cosa è verificato oggi**: **2.420 test reali passano, 0 fallimenti** (subset curato dell'intera suite Redis-derivata, eseguito in CI su ogni push — vedi `.github/workflows/`; l'intera suite non curata gira ~6h, quindi la CI ne esegue un sottoinsieme deliberato, non tutto). Non è ancora stato ripetuto per questo fork il tipo di benchmark indipendente reale (vs. Redis stock, hardware diverso da quello di sviluppo) fatto per `nexcache-VERAM3.3` — vedi quel repository per l'unico confronto di performance di questa famiglia di progetti attualmente verificato in questo modo.

**Nota di onestà**: le percentuali di impatto per-pillar più sotto (es. "riduzione cicli CPU", "riduzione latenza") descrivono l'obiettivo di design di ciascuna ottimizzazione, non un benchmark misurato e riproducibile per questo repository — a differenza dei numeri di `nexcache-VERAM3.3`, che sono legati a un'issue GitHub dedicata e a una run di benchmark reale. Non li ho cancellati (documentano l'intento architetturale), ma vanno letti come tali.

---

## ⚙️ Deep Dive Tecnico: I 4 Pilastri di SOVEREIGN

### Pillar 1: Speculative Metadata Filtering (SMF)
*   **Design & Struttura:** Abbiamo implementato un interceptor probabilistico (1MB Bloom-style Bitset) integrato direttamente nella funzione `lookupKey`.
*   **Implementazione:** La funzione `Sovereign_SpeculativeMiss(key)` calcola l'hash della chiave prima di accedere alla Hash Table principale. Se il bit corrispondente nel filtro è 0, la funzione restituisce un "Definite Miss" immediato.
*   **Impatto:** Elimina i costosi prober di memoria per chiavi inesistenti, riducendo i cicli CPU sprecati dell'87% durante carichi di scrittura massivi.

### Pillar 2: Hardware DNA Sensing
*   **Design & Struttura:** Un sistema di telemetria hardware che agisce durante la fase di boot (`Sovereign_SenseDNA`).
*   **Implementazione:** Utilizza chiamate `sysctlbyname` (per Apple Silicon AMX) e flag di compilazione per **ARM SVE2** (Rubin/Grace). Il sistema mappa il "DNA" dell'host nella variabile globale `server_dna`.
*   **Impatto:** Permette al kernel di NexCache di selezionare dinamicamente i percorsi di ottimizzazione vettoriale più efficienti per il parsing RESP e la manipolazione dei bit.

### Pillar 3: Circadian Memory Gardener (CMG)
*   **Design & Struttura:** Un'estensione della struttura `serverObject` che introduce il campo **`vitality`** (8-bit).
*   **Implementazione:** 
    * `Sovereign_ReinforceSynapse`: Incrementa la vitalità ad ogni accesso (hit).
    * `Sovereign_GardenerLoop`: Esegue un decadimento semantico graduale (16 shard/ciclo) durante il `serverCron`.
    * `Sovereign_GetEvictionScore`: Fornisce un punteggio di sfratto inverso al modulo `evict.c`, prioritizzando la rimozione dei dati con bassa attività "vitale".
*   **Specifiche Rubin-Mode:**
    *   **Allineamento:** 256-byte strict alignment per ogni `robj`.
    *   **Payload SVI:** 239 byte in-situ per stringhe embedded (riduzione latenza 45%).
    *   **Rilevamento Dinamico:** Sensing CPU via `__builtin_cpu_supports` per prevenire Illegal Instructions.
*   **Impatto:** Gestione della memoria biologica che mantiene i dati "importanti" più a lungo rispetto alla semplice logica LRU/LFU.

### Pillar 4: Associative Graphing (Synaptic Map)
*   **Design & Struttura:** Una **Synaptic Map** a 2048 slot che modella le relazioni spaziali tra le chiavi.
*   **Implementazione:** Tramite `Sovereign_LinkKeys`, il sistema registra le associazioni (es. chiavi richieste nella stessa operazione `MGET`). La funzione `Sovereign_PrefetchAssociates` analizza la forza sinaptica e innesca recuperi speculativi.
*   **Impatto:** Riduzione della latenza predittiva, preparando i dati associati in cache prima ancora che il client ne faccia richiesta esplicita.

---

### Chi sono: Giuseppe Lobbene
Sono **Giuseppe Lobbene**, ingegnere del software appassionato di architetture a bassa latenza. La mia storia riflette la ricerca di eccellenza tecnologica in un mercato spesso rigido. Questo progetto è la mia **"firma"**: una dimostrazione di come la passione, unita allo studio profondo del silicio, possa superare i limiti del computing tradizionale. Il mio motore è la mia famiglia e mio figlio **Oliver**, a cui dedico questa ricerca di innovazione costante.

---

<a name="english"></a>
## 🇺🇸 English Version

### Project Vision and Goal
**NexCache-SOVEREIGN (VERAM 4.0)** is a Redis fork in C experimenting with an adaptive optimization layer aimed at **NVIDIA Rubin-class** hardware (boot-time hardware sensing, speculative miss filtering, "biological" eviction instead of plain LRU/LFU). It's the direct follow-up to [nexcache-VERAM3.3](https://github.com/lobbenedesign/nexcache-VERAM3.3), sharing the same 4 conceptual pillars below in a newer design.

**What's verified today**: **2,420 real tests pass, 0 failures** (a curated subset of the full Redis-derived suite, run in CI on every push — see `.github/workflows/`; the full, un-curated suite takes ~6h, so CI runs a deliberate subset, not everything). This fork hasn't yet been through the kind of independent real benchmark (vs. stock Redis, on hardware different from the dev machine) done for `nexcache-VERAM3.3` — see that repository for the only performance comparison in this project family currently verified that way.

**Honesty note**: the per-pillar impact percentages below (e.g. "CPU cycle reduction", "latency reduction") describe each optimization's design target, not a measured, reproducible benchmark for this repository — unlike `nexcache-VERAM3.3`'s numbers, which are tied to a dedicated GitHub issue and a real benchmark run. They haven't been deleted (they document the architectural intent), but should be read as such.

---

## ⚙️ Technical Deep Dive: The 4 Pillars of SOVEREIGN

### Pillar 1: Speculative Metadata Filtering (SMF)
*   **Design & Structure:** We implemented a probabilistic interceptor (1MB Bloom-style Bitset) integrated directly into the `lookupKey` function.
*   **Implementation:** The `Sovereign_SpeculativeMiss(key)` function calculates the key's hash before accessing the main Hash Table. If the corresponding bit in the filter is 0, the function returns an immediate "Definite Miss."
*   **Impact:** Eliminates expensive memory probes for non-existent keys, reducing wasted CPU cycles by 87% during massive write loads.

### Pillar 2: Hardware DNA Sensing
*   **Design & Structure:** A hardware telemetry system that acts during the boot phase (`Sovereign_SenseDNA`).
*   **Implementation:** Uses `sysctlbyname` calls (for Apple Silicon AMX) and compiler flags for **ARM SVE2** (Rubin/Grace). The system maps the host's "DNA" into the global `server_dna` variable.
*   **Impact:** Allows the NexCache kernel to dynamically select the most efficient vector optimization paths for RESP parsing and bit manipulation.

### Pillar 3: Circadian Memory Gardener (CMG)
*   **Design & Structure:** An expansion of the `serverObject` structure introducing the **`vitality`** field (8-bit).
*   **Implementation:** 
    * `Sovereign_ReinforceSynapse`: Increases vitality on every access (hit).
    * `Sovereign_GardenerLoop`: Executes gradual semantic decay (16 shards/cycle) during `serverCron`.
    * `Sovereign_GetEvictionScore`: Provides an eviction score to the `evict.c` module, prioritizing the removal of data with low "vital" activity.
*   **Rubin-Mode Specifications:**
    *   **Alignment:** 256-byte strict alignment for every `robj`.
    *   **SVI Payload:** 239 bytes in-situ for embedded strings (45% latency reduction).
    *   **Dynamic Sensing:** CPU feature detection via `__builtin_cpu_supports` to prevent Illegal Instructions.
*   **Impact:** Biological memory management that keeps "important" data longer than simple LRU/LFU logic.

### Pillar 4: Associative Graphing (Synaptic Map)
*   **Design & Structure:** A 2048-slot **Synaptic Map** that models spatial relationships between keys.
*   **Implementation:** Through `Sovereign_LinkKeys`, the system records associations (e.g., keys requested in the same `MGET` operation). The `Sovereign_PrefetchAssociates` function analyzes synaptic strength and triggers speculative retrievals.
*   **Impact:** Predictive latency reduction, preparing associated data in the cache even before the client makes an explicit request.

---

### About Me: Giuseppe Lobbene
I am **Giuseppe Lobbene**, a software engineer passionate about low-latency architectures. This project is my **"signature"**: a demonstration of how passion, combined with deep silicon research, can push the boundaries of traditional computing. My engine is my family and my son **Oliver**, to whom I dedicate this constant search for innovation.

---
*Created with sovereignty and passion by Giuseppe Lobbene.*
