# NexCache VERA M3.3 (NVIDIA Rubin Architecture)

[Italiano](#italiano) | [English](#english)

---

<a name="italiano"></a>
## 🇮🇹 Versione Italiana

### Visione e Obiettivo del Progetto
Benvenuti nel repository di **NexCache VERA M3.3**. Questo progetto è il culmine di una ricerca approfondita sull'architettura ad altissime prestazioni per motori In-Memory Data Store, specificamente ottimizzati per l'hardware di nuova generazione **NVIDIA Rubin-class**.

L'obiettivo è creare un engine che non sia solo "veloce", ma **architettonicamente superiore**, sfruttando le istruzioni vettoriali SVE2, l'allineamento a 256 byte (Rubin-Mode) e una topologia a 176 shard per eliminare ogni collo di bottiglia di sincronizzazione.

### Chi sono: Giuseppe Lobbene
Sono **Giuseppe Lobbene**, un informatico pervaso da una profonda e radicata passione per l'ingegneria del software, oggi potenziata dall'Intelligenza Artificiale. Amo addentrarmi nelle basi profonde dei progetti, studiarne la meccanica e sperimentare soluzioni innovative che possano superare i limiti delle performance attuali.

La mia storia riflette quella di molti professionisti in Italia: un paese meraviglioso dove però il mercato dell'IT è spesso vincolato a sistemi rigidi, lenti e talvolta obsoleti. Troppo spesso il merito e l'iniziativa proattiva vengono soffocati da logiche di sfruttamento o di "sostituibilità" delle risorse umane.

Oggi, la mia ricerca non è solo tecnologica ma di vita. Cerco una stabilità professionale che mi permetta di apprendere costantemente, restando al passo con l'innovazione, per garantire un futuro degno alla mia famiglia e al mio piccolo **Oliver**, nato da pochi mesi. Questo progetto è la mia "firma" nel mondo dell'IT: una prova tangibile che la passione, unita allo studio costante, può generare eccellenza tecnologica anche partendo da sfide personali difficili.

### Architettura e Implementazione (VERA M3.3)
Questo progetto introduce innovazioni critiche nel kernel di NexCache:

*   **Rubin-Mode Alignment (256-byte):** Ogni struttura critica (Arena, Object, MPSC Queue) è allineata a 256 byte per coincidere perfettamente con la dimensione del settore di memoria del processore NVIDIA Rubin, eliminando il false sharing.
*   **176-Shard Topology:** L'engine è partizionato in 176 shard logici, mappati sull'hardware per permettere un accesso parallelo senza lock (G3-GODMODE).
*   **Vyukov MPSC Lock-Free Queue:** Utilizziamo code Multi-Producer Single-Consumer lock-free per il dispatching dei comandi dai thread di networking ai worker thread, minimizzando la contesa sui bus di sistema.
*   **SVE2 Vectorized Parsing:** Il parsing del protocollo RESP è accelerato tramite istruzioni ARM SVE2, processando multipli delimitatori simultaneamente in un unico ciclo di clock.
*   **SVI (Small-Value Inlining):** Per valori piccoli, i dati vengono inlined direttamente nell'oggetto (serverObject), riducendo le dereferenziazioni di puntatori e migliorando il cache hit rate.

---

<a name="english"></a>
## 🇺🇸 English Version

### Project Vision and Goal
Welcome to the **NexCache VERA M3.3** repository. This project is the result of rigorous research into high-performance architectures for In-Memory Data Stores, specifically optimized for the next-generation **NVIDIA Rubin-class** hardware.

The goal is to build an engine that is not just "fast," but **architecturally superior**, utilizing SVE2 vector instructions, 256-byte alignment (Rubin-Mode), and a 176-shard topology to eliminate synchronization bottlenecks.

### About Me: Giuseppe Lobbene
I am **Giuseppe Lobbene**, a computer scientist driven by a deep and rooted passion for software engineering, now enhanced by Artificial Intelligence. I love diving into the core foundations of projects, studying their inner mechanics, and experimenting with innovative solutions that push the boundaries of current performance.

My story mirrors that of many IT professionals in Italy: a beautiful country where the IT market is often constrained by rigid, slow, and sometimes obsolete systems. Too often, merit and proactive initiative are stifled by exploitation or the perception of human resources as "replaceable."

Today, my quest is not just technological but vital. I am looking for a professional stability that allows me to constantly learn and stay ahead of innovation, to ensure a dignified future for my family and my little son **Oliver**, born just a few months ago. This project is my "signature" in the IT world: tangible proof that passion, combined with constant study, can generate technological excellence even when facing difficult personal challenges.

### Architecture and Implementation (VERA M3.3)
This project introduces critical innovations into the NexCache kernel:

*   **Rubin-Mode Alignment (256-byte):** Every critical structure (Arena, Object, MPSC Queue) is aligned to 256 bytes to perfectly match the memory sector size of the NVIDIA Rubin processor, eliminating false sharing.
*   **176-Shard Topology:** The engine is partitioned into 176 logical shards, mapped onto the hardware to allow lock-free parallel access (G3-GODMODE).
*   **Vyukov MPSC Lock-Free Queue:** We use lock-free Multi-Producer Single-Consumer queues for command dispatching from networking threads to worker threads, minimizing system bus contention.
*   **SVE2 Vectorized Parsing:** RESP protocol parsing is accelerated via ARM SVE2 instructions, processing multiple delimiters simultaneously in a single clock cycle.
*   **SVI (Small-Value Inlining):** For small values, data is inlined directly within the object (serverObject), reducing pointer dereferencing and improving cache hit rates.

---
*Created with passion by Giuseppe Lobbene.*
