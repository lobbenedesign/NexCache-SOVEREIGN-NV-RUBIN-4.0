# NexCache Core 1.0 — Performance Showcase

![NexCache Banner](https://img.shields.io/badge/NexCache-1.0-blue?style=for-the-badge&logo=cplusplus)
![Status](https://img.shields.io/badge/Status-Performance_Verified-orange?style=for-the-badge)

[English Version Below](#english-version) | [Versione Italiana](#versione-italiana)

---

<a name="english-version"></a>
## 🚀 Verified Performance Results (English)

NexCache is a high-performance in-memory database engine designed to overcome the fundamental limits of legacy key-value stores. This repository serves as a **Performance Showcase**, providing verified benchmark results and tools to compare NexCache against industry standards like Redis and Valkey.

### 📊 Benchmark Summary

| Metric | Redis 8.x | Valkey 9.x | **NexCache 1.0** |
| :--- | :---: | :---: | :---: |
| **Throughput (SET/GET)** | 1.0x (Base) | 1.1x | **✅ 1.8x - 2.4x** |
| **P99 Latency** | ~500μs | ~450μs | **✅ < 100μs** |
| **Memory Efficiency** | High | Medium | **✅ Ultra-Low (NexDash)** |
| **Entry Overhead** | 64 byte | 64 byte | **✅ 16-24 byte** |

### 🛠️ How to Verify
Use the provided scripts in the `benchmarks/` directory to run comparisons in your environment.
The source code for the intelligent storage layer is currently protected for enterprise evaluation. For access to the full core implementation or a trial binary, please reach out via the contact information below.

---

<details>
<summary><b>🔍 Behind the Engine: A Note from the Author / Nota dell'Autore</b></summary>

### **English: About the Author**

My name is **Giuseppe Lobbene**, and I am a developer who believes in the power of architectural elegance and the necessity of constant innovation.

In developing **NexCache**, I’ve tried to follow in the footsteps of innovators like **Salvatore Sanfilippo**, who revolutionized the tech world by solving problems that others hadn't even recognized. My goal is to push the boundaries of performance and efficiency while building a solid foundation for my family.

I am currently seeking a high-level professional challenge—a role in a tech-driven company that values expertise and offers the stability needed to build a secure future for my son. My professional journey has been intense:
*   I managed the technical operations of a beach booking startup that reached €300,000 in transactions in its first season.
*   I handled everything from B2B/B2C support and graphic design to developing custom **Flutter apps** and system configurations.
*   Despite working as a freelancer full-time (7 days a week) for years with very limited pay, I gave 100% as if the company were my own.

Currently, I work for a supportive construction company that allows me to work remotely from my home in **Puglia**. While I am grateful, I hope one day to bring my deep technical know-how to a purely tech-focused enterprise with ambitious goals.

If you are looking for a dedicated engineer who builds with passion and responsibility, let's talk.

---

### **Italiano: Nota dell'Autore**

Mi chiamo **Giuseppe Lobbene**, e sono uno sviluppatore che crede nell'eleganza dell'architettura software e nell'innovazione costante.

Con **NexCache**, ho cercato di seguire (seppur "maldestramente") le orme di **Salvatore Sanfilippo**, una figura che ha dato tanto alla comunità informatica mondiale risolvendo problemi che altri non avevano nemmeno iniziato a porsi.

Oggi sono alla ricerca di una sfida professionale di alto livello — una realtà tecnologica che valorizzi le competenze e offra la stabilità necessaria per dare un futuro degno a mio figlio e una casa alla mia famiglia. Il mio percorso non è stato facile:
*   Ho preso in mano la gestione di una startup per la prenotazione spiagge, portandola a 300.000€ di transato fin dalla prima stagione.
*   Mi sono occupato di tutto: supporto B2B/B2C, design grafico, configurazione gestionali e sviluppo di **app Flutter**.
*   Ho lavorato per anni come autonomo a partita IVA, 7 giorni su 7, con compensi che non rispecchiavano l'impegno profuso, trattando i progetti come se fossero di mia proprietà.

Al momento lavoro in smartworking dalla **Puglia** per un'azienda edile che mi ha accolto calorosamente. Tuttavia, trovandomi lontano dal mondo dell'innovazione tecnologica pura, spero un giorno di entrare a far parte di un'azienda con grandi mire, dove possa crescere e dare valore alla mia figura professionale.

Se cerchi uno sviluppatore che costruisce con passione e un profondo senso di responsabilità, contattami.

📫 Reach out: [giuseppelobbene@gmail.com](mailto:giuseppelobbene@gmail.com)
</details>

<a name="versione-italiana"></a>
## 🚀 Risultati Prestazionali Verificati (Italiano)

NexCache è un engine di storage in-memory ad altissime prestazioni. Questo repository funge da **Performance Showcase**, fornendo i risultati dei benchmark e gli strumenti per confrontare NexCache con gli standard di mercato.

### 📈 Confronto Diretto
Il nostro engine dimostra una superiorità netta nella gestione della memoria (fino al 40% di risparmio) e nella latenza di risposta, grazie all'architettura a bassa frammentazione e al sistema di metadati densi.

---
*NexCache — Rethink Memory, Accelerate AI.*
