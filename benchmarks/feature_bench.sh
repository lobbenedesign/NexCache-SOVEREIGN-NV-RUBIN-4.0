#!/bin/bash
# NexCache v4 Feature Benchmark
# ============================================================
# Misura le performance delle nuove feature: Subkey TTL, CRDT, VLL.
#
# Copyright (c) 2026 NexCache Project — BSD License

set -e

# Configurazione
NEXCACHE_HOST="127.0.0.1"
NEXCACHE_PORT="6381"
N_REQUESTS=100000
N_CLIENTS=50

# Colori
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}NexCache v4 Advanced Feature Benchmark${NC}"
echo "============================================================"

# Verifica se il server è attivo
if ! nexcache-cli -p $NEXCACHE_PORT ping > /dev/null 2>&1; then
    echo "Errore: NexCache non in esecuzione sulla porta $NEXCACHE_PORT"
    exit 1
fi

run_bench() {
    local title=$1
    local cmd=$2
    echo -ne "  %-40s " "$title"
    res=$(nexcache-benchmark -p $NEXCACHE_PORT -n $N_REQUESTS -c $N_CLIENTS -q $cmd 2>&1 | grep "requests per second" | awk '{print $1}')
    echo -e "${GREEN}$res req/s${NC}"
}

echo -e "\n${YELLOW}1. CRDT Commands Throughput${NC}"
run_bench "GCounter Increment (GINCR)" "GINCR counter:1 1"
run_bench "GCounter Read (GCOUNTER_GET)" "GCOUNTER_GET counter:1"
run_bench "PN-Counter Increment (PNINCR)" "PNINCR pn:1 1"
run_bench "OR-Set Add (ORSET.ADD)" "ORSET.ADD set:1 member"

echo -e "\n${YELLOW}2. Subkey TTL Performance${NC}"
# Pre-popola una hash
nexcache-cli -p $NEXCACHE_PORT HSET hash:1 f1 v1 f2 v2 f3 v3 > /dev/null
run_bench "Subkey Expire (HEXPIRE)" "HEXPIRE hash:1 60 FIELDS 1 f1"
run_bench "Subkey TTL Read (HTTL)" "HTTL hash:1 FIELDS 1 f1"

echo -e "\n${YELLOW}3. Dual-Store Latency Check${NC}"
run_bench "String SET (Main Store)" "SET key:__rand_int__ val"
run_bench "Hash HSET (Object Store)" "HSET hash:__rand_int__ f v"

echo -e "\n${YELLOW}4. Anomaly Detection Overhead${NC}"
# Disabilita/Abilita per vedere la differenza (opzionale - qui misuriamo solo attivo)
run_bench "GET (with Anomaly Tracking)" "GET key:1"

echo -e "\n============================================================"
echo -e "${GREEN}Benchmark Complete${NC}"
