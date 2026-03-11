# NexCache Core 1.0 — The Intelligent In-Memory Storage Layer

![NexCache Banner](https://img.shields.io/badge/NexCache-1.0-blue?style=for-the-badge&logo=cplusplus)
![License](https://img.shields.io/badge/License-BSD_3--Clause-green?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Production_Ready-orange?style=for-the-badge)

[English Version Below](#english-version) | [Versione Italiana](#versione-italiana)

---

<a name="english-version"></a>
## 🚀 Why NexCache? (English)

In a world where cloud RAM costs continue to soar and AI applications demand microsecond responses, traditional databases show their limits: **fragmentation**, **metadata overhead**, and **inefficient networking**.

**NexCache** is the new standard for high-performance in-memory databases. Designed as a philosophical evolution of Redis-compatible systems, it solves these issues at the root.

### Core Innovations:
1.  **Narrow-Waist C++ Architecture**: Zero Garbage Collection, zero latencies. Native Arena Allocator eliminates fragmentation and ensures cache-locality.
2.  **Meta-Densed Hash Table (NexDash)**: Ultra-compact entry design (**24 bytes** vs 64 bytes in Redis), enabling up to **40% more data** per server.
3.  **Autonomous Vector Routing**: Real-time hardware detection (AVX-512, NEON, SVE) to select the optimal SIMD kernels for AI vector processing.
4.  **NexSegcache (8-byte Item Body)**: A state-of-the-art storage backend that reduces per-item overhead to 8 bytes, saving up to 60% memory compared to traditional stores.

## 📊 Market Comparison

| Feature | Redis 8.x | Valkey 9.x | Dragonfly | **NexCache 1.0** |
| :--- | :---: | :---: | :---: | :---: |
| **Overhead per entry** | 64 byte | 64 byte | 20 byte | **✅ 16-24 byte** |
| **Storage Body** | 64 byte | 56 byte | 24 byte | **✅ 8 byte (Segcache Parent)** |
| **Subkey TTL** | No | Hash only | Set only | **✅ Universal** |
| **Native CRDTs** | No | No | No | **✅ Included** |
| **Networking** | Epoll | RDMA (Exp) | Epoll | **✅ Dual-Path (TCP/RDMA)** |
| **Memory Management** | malloc | jemalloc | Segcache | **✅ Arena Allocator** |

## 💎 Industry Firsts (Only in NexCache)

*   **Universal Subkey TTL**: Set expiration times on individual fields within Hashes, Sets, ZSets, and JSON. 
*   **Active-Active CRDTs**: Built-in Conflict-free Replicated Data Types for multi-master global replication.
*   **Vector Auto-Quantization**: Automatic transition between FP32, INT8, and Binary quantization based on SIMD availability.

---

<a name="versione-italiana"></a>
## � Perché NexCache? (Italiano)

In un mondo dove il costo della RAM cloud continua a salire e le applicazioni AI richiedono risposte in microsecondi, i database tradizionali mostrano i loro limiti: **frammentazione**, **overhead dei metadati** e **networking inefficiente**.

**NexCache** è il nuovo standard per i database in-memory ad alte prestazioni, risolvendo alla radice i colli di bottiglia dei sistemi legacy.

### Innovazioni Fondamentali:
1.  **Architettura Narrow-Waist C++**: Zero Garbage Collection, zero latenze imprevedibili. Arena Allocator nativo per eliminare la frammentazione.
2.  **Meta-Densed Hash Table (NexDash)**: Entry ridotte a **24 byte**, permettendo di caricare fino al **40% di dati in più** nello stesso server.
3.  **Routing Vettoriale Autonomo**: Rileva a runtime le capacità della CPU (AVX-512, NEON, SVE) e sceglie il kernel SIMD ottimale.
4.  **NexSegcache (8-byte Item Body)**: Backend di storage che riduce l'overhead per singolo elemento a soli 8 byte.

## 🏁 How to Start / Come Iniziare (Quickstart)

```bash
git clone https://github.com/giuseppelobbene-source/nexcache-core1.git
cd nexcache-core1
make all
./build/nexcache-server
```

## 📜 License
NexCache is released under the **BSD 3-Clause License**.

---
*NexCache — Rethink Memory, Accelerate AI.*

<details>
<summary><b>🔍 Behind the Engine: A Note from the Author / Nota dell'Autore</b></summary>

**English:**
Developing NexCache was more than a technical challenge; it was an act of pure dedication. 
I am a developer who believes in the power of architectural elegance and the necessity of constant growth. If you find value in this project, you've seen a glimpse of my professional drive. Currently, I am seeking new stimulating opportunities that allow me to push my boundaries even further. 

My primary mission is to build a solid future for my family—to secure a stable home and provide the best possible environment for my son to thrive. I am looking for a professional environment that values deep technical problem-solving and offers the stability and growth needed to provide for my loved ones.

**Italiano:**
Sviluppare NexCache è stato più di una sfida tecnica; è stato un atto di pura dedizione.
Sono uno sviluppatore che crede nell'eleganza architetturale e nella necessità di una crescita costante. Se trovi valore in questo progetto, hai visto un riflesso della mia determinazione professionale. Attualmente, sto cercando nuove opportunità stimolanti che mi permettano di superare i miei limiti.

La mia missione principale è costruire un futuro solido per la mia famiglia — garantire una casa stabile e fornire il miglior ambiente possibile affinché mio figlio possa crescere e prosperare. Cerco un ambiente professionale che dia valore alla risoluzione di problemi tecnici complessi e offra la stabilità e la crescita necessarie per provvedere ai miei cari.

If you are looking for a developer who builds with both passion and a deep sense of responsibility, I'd love to connect.
Se cerchi uno sviluppatore che costruisce con passione e un profondo senso di responsabilità, mi piacerebbe parlarne.

📫 Reach out: [giuseppelobbene@gmail.com](mailto:giuseppelobbene@gmail.com)
</details>
