# NexCache v5.0 — Guida Emulazione RDMA & CXL
*Hardware e Cloud Testing Guide per ambienti non dotati di InfiniBand e PCIe 5.*

Uno dei punti deboli evidenziati dall'analisi architetturale concorrente (Valkey, Redis, Dragonfly) era la dipendenza da hw specializzato per raggiungere prestazioni ultra-low latency (< 20μs). 

Per mitigare questo rischio d'adozione, **NexCache** integra moduli di emulazione RDMA e script per il testing dell'architettura in ambienti cloud commodity.

---

## 1. Emulazione RDMA (Soft-RoCE / rdma_rxe)

La Replica Delta Zero-CPU usa primitive `ibv_post_send` che scrivono le modifiche CRDT bypassando il kernel target. Chi sta sviluppando o debuggando senza schede di rete InfiniBand/RoCE fisiche può usare il nostro stack emulato.

### A. Avvio Sub-sistema RDMA Emulato
Eseguire lo script fornito nella cartella `scripts`:
```bash
sudo ./scripts/setup_rdma_emulation.sh
```
**Cosa fa lo script:**
1. Installa i pacchetti `ibverbs-utils`, `rdmacm-utils`.
2. Carica il modulo kernel `rdma_rxe` (Software RoCE).
3. Esegue un binding del driver fittizio (`rxe0`) sulla prima interfaccia ethernet disponbile o sul link locale.
4. Espone la periferica RDMA emulata al livello applicazione.

### B. Mappatura NexCache
Passare il flag all'esecuzione del server o nel config `nexcache.conf`:
```bash
./nexcache-server --rdma-device rxe0
```

Se il dispositivo fallback fallisce per colpa del container Docker, NexCache attiverà nativamente il layer `Auto-Fallback Protocol`, switchando a `io_uring` e comunicando il warning in console.

---

## 2. Emulazione Latenze CXL (NUMA Node Binding)

L'integrazione di **Compute Express Link (CXL)** per gestire i blocchi freddi (TimeSeries o vettori compressi HNSW) a grandi distanze è testabile sfruttando i controller della memoria NUMA, allontanando deliberatamente i pacchetti per simulare le tempistiche PCIe (paper VLDB 2025).

### Simuliamo la latenza Memory Tiering (T1-T4)
Su una macchina multi-socket (come un AWS M6i bare-metal o istanza dual-cpu):
```bash
# Isola l'arena allocation di NexCache nel nodo NUMA 1,
# mentre i core di esecuzione restano nel nodo NUMA 0.
numactl --cpunodebind=0 --membind=1 ./nexcache-server --tiering-mode simulated-cxl
```
Questo simula in hardware le latenze d'accesso di ~200-300 ns tipiche dei memory controller remotizzati via cavo PCIe 5, permettendoti di testare il comportamento di memory-swapping asincrono di `NexDashTable`.

---

## 3. Benchmark via Commodity Cloud (io_uring)

NexCache non perde il target prestazionale neppure senza kernel-bypass hardware. Per provare l'efficienza C++ e dell'allocatore `Arena`, testiamo in ambienti CPU generici:

**Metodo Consigliato:**
Lanciare le AWS EC2 ARM-based (Graviton3 o Graviton4), come `c7g` o `c6gn`, con Linux OS e `io_uring` supportato.
```bash
./scripts/commodity_benchmark.sh
```
Mostrerà come l'assenza del parsing RESP in single-thread (tipico di Valkey) consenta un throughput di rete **+1.2M QPS** tramite io_uring per le connessioni standard TCP/epoll aggregate su pipeline HTTP/2 e RESP3.
