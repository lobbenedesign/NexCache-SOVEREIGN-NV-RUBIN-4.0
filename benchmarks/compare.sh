#!/bin/bash
# NexCache — Benchmark ufficiale vs NexCache vs NexCache
# ============================================================
# Esegui PRIMA di ogni release per misurare le performance.
# Pubblica i risultati nel README.md del progetto.
#
# Prerequisiti:
#   - nexcache-benchmark installato
#   - Docker con nexcache:latest e nexcache/nexcache:latest
#   - NexCache server in esecuzione
#
# Usage:
#   ./compare.sh [--quick] [--full] [--port 6379]
#
# Copyright (c) 2026 NexCache Project — BSD License

set -e

# ── Configurazione ──────────────────────────────────────────
NEXCACHE_HOST="${NEXCACHE_HOST:-127.0.0.1}"
NEXCACHE_PORT="${NEXCACHE_PORT:-6379}"
NEXCACHE_PORT="${NEXCACHE_PORT:-6380}"
NEXCACHE_PORT="${NEXCACHE_PORT:-6381}"

N_REQUESTS=1000000        # Operazioni per benchmark
N_CLIENTS=50              # Client simultanei
N_PIPELINE=10             # Pipeline depth
QUICK_MODE=0
FULL_MODE=0

# Colori output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

# Parse args
for arg in "$@"; do
    case "$arg" in
        --quick) QUICK_MODE=1; N_REQUESTS=100000 ;;
        --full)  FULL_MODE=1; N_REQUESTS=5000000 ;;
        --port*) NEXCACHE_PORT="${arg#*=}" ;;
    esac
done

# ── Utility ─────────────────────────────────────────────────
check_server() {
    local host=$1
    local port=$2
    local name=$3
    if nexcache-cli -h "$host" -p "$port" ping &>/dev/null; then
        echo -e "  ${GREEN}✅ $name running on $host:$port${NC}"
        return 0
    else
        echo -e "  ${RED}❌ $name NOT running on $host:$port${NC}"
        return 1
    fi
}

run_bench() {
    local name=$1
    local port=$2
    local test_type=$3
    local extra_args="${4:-}"

    nexcache-benchmark \
        -h "$NEXCACHE_HOST" \
        -p "$port" \
        -n "$N_REQUESTS" \
        -c "$N_CLIENTS" \
        -P "$N_PIPELINE" \
        -t "$test_type" \
        --csv \
        $extra_args 2>/dev/null | \
    awk -F',' -v name="$name" '{
        gsub(/"/, "", $1);
        gsub(/"/, "", $2);
        printf "  %-20s %-30s %10s req/s\n", name, $1, $2
    }'
}

# ── Header ──────────────────────────────────────────────────
echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║         NexCache Benchmark Suite vs NexCache vs NexCache          ║${NC}"
echo -e "${BOLD}║              $(date '+%Y-%m-%d %H:%M:%S')                          ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Check server status ──────────────────────────────────────
echo -e "${CYAN}── Server Status ────────────────────────────────────────────${NC}"
NEXCACHE_OK=0; NEXCACHE_OK=0; NEXCACHE_OK=0
check_server "$NEXCACHE_HOST" "$NEXCACHE_PORT" "NexCache" && NEXCACHE_OK=1
check_server "$NEXCACHE_HOST" "$NEXCACHE_PORT" "NexCache" && NEXCACHE_OK=1
check_server "$NEXCACHE_HOST" "$NEXCACHE_PORT" "NexCache" && NEXCACHE_OK=1
echo ""

# ── Benchmark 1: SET/GET throughput ─────────────────────────
echo -e "${CYAN}── BENCHMARK 1: SET/GET Throughput ($N_REQUESTS ops, $N_CLIENTS clients) ──${NC}"
echo -e "  ${YELLOW}Server               Test                           Throughput${NC}"
echo "  ─────────────────────────────────────────────────────────────"

[[ $NEXCACHE_OK    -eq 1 ]] && run_bench "NexCache 8.6.1"    "$NEXCACHE_PORT"    "set,get"
[[ $NEXCACHE_OK   -eq 1 ]] && run_bench "NexCache"         "$NEXCACHE_PORT"   "set,get"
[[ $NEXCACHE_OK -eq 1 ]] && run_bench "NexCache"       "$NEXCACHE_PORT" "set,get"

# ── Benchmark 2: Latenza P50/P99/P999 ───────────────────────
echo ""
echo -e "${CYAN}── BENCHMARK 2: Latency P50/P99 (100k ops, 100 clients) ────────${NC}"
echo ""

bench_latency() {
    local name=$1
    local port=$2

    output=$(nexcache-benchmark \
        -h "$NEXCACHE_HOST" \
        -p "$port" \
        -n 100000 \
        -c 100 \
        -t get \
        --latency-history 2>/dev/null | tail -5)

    echo -e "  ${BOLD}$name:${NC}"
    echo "$output" | head -3 | sed 's/^/    /'
    echo ""
}

[[ $NEXCACHE_OK    -eq 1 ]] && bench_latency "NexCache 8.6.1"  "$NEXCACHE_PORT"
[[ $NEXCACHE_OK   -eq 1 ]] && bench_latency "NexCache"       "$NEXCACHE_PORT"
[[ $NEXCACHE_OK -eq 1 ]] && bench_latency "NexCache"     "$NEXCACHE_PORT"

# ── Benchmark 3: Pipeline throughput ────────────────────────
echo -e "${CYAN}── BENCHMARK 3: Pipeline SET (pipeline=16) ─────────────────────${NC}"
echo -e "  ${YELLOW}Server               Test                           Throughput${NC}"
echo "  ─────────────────────────────────────────────────────────────"

[[ $NEXCACHE_OK    -eq 1 ]] && run_bench "NexCache 8.6.1" "$NEXCACHE_PORT"    "set" "-P 16"
[[ $NEXCACHE_OK   -eq 1 ]] && run_bench "NexCache"      "$NEXCACHE_PORT"   "set" "-P 16"
[[ $NEXCACHE_OK -eq 1 ]] && run_bench "NexCache"    "$NEXCACHE_PORT" "set" "-P 16"

# ── Benchmark 4: Memoria ────────────────────────────────────
echo ""
echo -e "${CYAN}── BENCHMARK 4: Memoria usata (10M chiavi × 100 bytes) ─────────${NC}"

bench_memory() {
    local name=$1
    local port=$2

    # Inserisci 10M chiavi da 100 bytes
    nexcache-cli -h "$NEXCACHE_HOST" -p "$port" FLUSHALL &>/dev/null
    nexcache-benchmark \
        -h "$NEXCACHE_HOST" \
        -p "$port" \
        -n 10000000 \
        -c 50 \
        -t set \
        -d 100 &>/dev/null

    used=$(nexcache-cli -h "$NEXCACHE_HOST" -p "$port" INFO memory 2>/dev/null | \
           awk -F: '/used_memory_human/{print $2}' | tr -d '\r')
    frag=$(nexcache-cli -h "$NEXCACHE_HOST" -p "$port" INFO memory 2>/dev/null | \
           awk -F: '/mem_fragmentation_ratio/{print $2}' | tr -d '\r')
    keys=$(nexcache-cli -h "$NEXCACHE_HOST" -p "$port" DBSIZE 2>/dev/null)

    printf "  %-20s keys=%-12s memory=%-12s frag_ratio=%s\n" \
           "$name" "$keys" "$used" "$frag"
}

[[ $NEXCACHE_OK    -eq 1 ]] && bench_memory "NexCache 8.6.1"  "$NEXCACHE_PORT"
[[ $NEXCACHE_OK   -eq 1 ]] && bench_memory "NexCache"       "$NEXCACHE_PORT"
[[ $NEXCACHE_OK -eq 1 ]] && bench_memory "NexCache"     "$NEXCACHE_PORT"

# ── Benchmark 5: Multi-core scaling ─────────────────────────
if [[ $FULL_MODE -eq 1 ]]; then
    echo ""
    echo -e "${CYAN}── BENCHMARK 5: Multi-core scaling ─────────────────────────────${NC}"
    echo -e "  ${YELLOW}Clients    NexCache req/s    NexCache req/s    NexCache req/s${NC}"
    echo "  ─────────────────────────────────────────────────────────────"

    for clients in 1 4 8 16 32 64 128; do
        nexcache_rps=""
        nexcache_rps=""
        nexcache_rps=""

        [[ $NEXCACHE_OK -eq 1 ]] && nexcache_rps=$(nexcache-benchmark -h "$NEXCACHE_HOST" \
            -p "$NEXCACHE_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        [[ $NEXCACHE_OK -eq 1 ]] && nexcache_rps=$(nexcache-benchmark -h "$NEXCACHE_HOST" \
            -p "$NEXCACHE_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        [[ $NEXCACHE_OK -eq 1 ]] && nexcache_rps=$(nexcache-benchmark -h "$NEXCACHE_HOST" \
            -p "$NEXCACHE_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        printf "  %-8s %-16s %-16s %-16s\n" \
               "${clients}c" \
               "${nexcache_rps:-N/A}" \
               "${nexcache_rps:-N/A}" \
               "${nexcache_rps:-N/A}"
    done
fi

# ── Report finale ────────────────────────────────────────────
echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║                    Benchmark Complete                        ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Results saved to: $(pwd)/benchmark_results_$(date '+%Y%m%d_%H%M%S').txt"
echo ""
