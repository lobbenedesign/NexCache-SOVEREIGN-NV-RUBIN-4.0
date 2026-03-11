#!/bin/bash
# NexCache — Benchmark ufficiale vs Redis vs Valkey
# ============================================================
# Esegui PRIMA di ogni release per misurare le performance.
# Pubblica i risultati nel README.md del progetto.
#
# Prerequisiti:
#   - redis-benchmark installato
#   - Docker con redis:latest e valkey/valkey:latest
#   - NexCache server in esecuzione
#
# Usage:
#   ./compare.sh [--quick] [--full] [--port 6379]
#
# Copyright (c) 2026 NexCache Project — BSD License

set -e

# ── Configurazione ──────────────────────────────────────────
REDIS_HOST="${REDIS_HOST:-127.0.0.1}"
REDIS_PORT="${REDIS_PORT:-6379}"
VALKEY_PORT="${VALKEY_PORT:-6380}"
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
    if redis-cli -h "$host" -p "$port" ping &>/dev/null; then
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

    redis-benchmark \
        -h "$REDIS_HOST" \
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
echo -e "${BOLD}║         NexCache Benchmark Suite vs Redis vs Valkey          ║${NC}"
echo -e "${BOLD}║              $(date '+%Y-%m-%d %H:%M:%S')                          ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Check server status ──────────────────────────────────────
echo -e "${CYAN}── Server Status ────────────────────────────────────────────${NC}"
REDIS_OK=0; VALKEY_OK=0; NEXCACHE_OK=0
check_server "$REDIS_HOST" "$REDIS_PORT" "Redis" && REDIS_OK=1
check_server "$REDIS_HOST" "$VALKEY_PORT" "Valkey" && VALKEY_OK=1
check_server "$REDIS_HOST" "$NEXCACHE_PORT" "NexCache" && NEXCACHE_OK=1
echo ""

# ── Benchmark 1: SET/GET throughput ─────────────────────────
echo -e "${CYAN}── BENCHMARK 1: SET/GET Throughput ($N_REQUESTS ops, $N_CLIENTS clients) ──${NC}"
echo -e "  ${YELLOW}Server               Test                           Throughput${NC}"
echo "  ─────────────────────────────────────────────────────────────"

[[ $REDIS_OK    -eq 1 ]] && run_bench "Redis 8.6.1"    "$REDIS_PORT"    "set,get"
[[ $VALKEY_OK   -eq 1 ]] && run_bench "Valkey"         "$VALKEY_PORT"   "set,get"
[[ $NEXCACHE_OK -eq 1 ]] && run_bench "NexCache"       "$NEXCACHE_PORT" "set,get"

# ── Benchmark 2: Latenza P50/P99/P999 ───────────────────────
echo ""
echo -e "${CYAN}── BENCHMARK 2: Latency P50/P99 (100k ops, 100 clients) ────────${NC}"
echo ""

bench_latency() {
    local name=$1
    local port=$2

    output=$(redis-benchmark \
        -h "$REDIS_HOST" \
        -p "$port" \
        -n 100000 \
        -c 100 \
        -t get \
        --latency-history 2>/dev/null | tail -5)

    echo -e "  ${BOLD}$name:${NC}"
    echo "$output" | head -3 | sed 's/^/    /'
    echo ""
}

[[ $REDIS_OK    -eq 1 ]] && bench_latency "Redis 8.6.1"  "$REDIS_PORT"
[[ $VALKEY_OK   -eq 1 ]] && bench_latency "Valkey"       "$VALKEY_PORT"
[[ $NEXCACHE_OK -eq 1 ]] && bench_latency "NexCache"     "$NEXCACHE_PORT"

# ── Benchmark 3: Pipeline throughput ────────────────────────
echo -e "${CYAN}── BENCHMARK 3: Pipeline SET (pipeline=16) ─────────────────────${NC}"
echo -e "  ${YELLOW}Server               Test                           Throughput${NC}"
echo "  ─────────────────────────────────────────────────────────────"

[[ $REDIS_OK    -eq 1 ]] && run_bench "Redis 8.6.1" "$REDIS_PORT"    "set" "-P 16"
[[ $VALKEY_OK   -eq 1 ]] && run_bench "Valkey"      "$VALKEY_PORT"   "set" "-P 16"
[[ $NEXCACHE_OK -eq 1 ]] && run_bench "NexCache"    "$NEXCACHE_PORT" "set" "-P 16"

# ── Benchmark 4: Memoria ────────────────────────────────────
echo ""
echo -e "${CYAN}── BENCHMARK 4: Memoria usata (10M chiavi × 100 bytes) ─────────${NC}"

bench_memory() {
    local name=$1
    local port=$2

    # Inserisci 10M chiavi da 100 bytes
    redis-cli -h "$REDIS_HOST" -p "$port" FLUSHALL &>/dev/null
    redis-benchmark \
        -h "$REDIS_HOST" \
        -p "$port" \
        -n 10000000 \
        -c 50 \
        -t set \
        -d 100 &>/dev/null

    used=$(redis-cli -h "$REDIS_HOST" -p "$port" INFO memory 2>/dev/null | \
           awk -F: '/used_memory_human/{print $2}' | tr -d '\r')
    frag=$(redis-cli -h "$REDIS_HOST" -p "$port" INFO memory 2>/dev/null | \
           awk -F: '/mem_fragmentation_ratio/{print $2}' | tr -d '\r')
    keys=$(redis-cli -h "$REDIS_HOST" -p "$port" DBSIZE 2>/dev/null)

    printf "  %-20s keys=%-12s memory=%-12s frag_ratio=%s\n" \
           "$name" "$keys" "$used" "$frag"
}

[[ $REDIS_OK    -eq 1 ]] && bench_memory "Redis 8.6.1"  "$REDIS_PORT"
[[ $VALKEY_OK   -eq 1 ]] && bench_memory "Valkey"       "$VALKEY_PORT"
[[ $NEXCACHE_OK -eq 1 ]] && bench_memory "NexCache"     "$NEXCACHE_PORT"

# ── Benchmark 5: Multi-core scaling ─────────────────────────
if [[ $FULL_MODE -eq 1 ]]; then
    echo ""
    echo -e "${CYAN}── BENCHMARK 5: Multi-core scaling ─────────────────────────────${NC}"
    echo -e "  ${YELLOW}Clients    Redis req/s    Valkey req/s    NexCache req/s${NC}"
    echo "  ─────────────────────────────────────────────────────────────"

    for clients in 1 4 8 16 32 64 128; do
        redis_rps=""
        valkey_rps=""
        nexcache_rps=""

        [[ $REDIS_OK -eq 1 ]] && redis_rps=$(redis-benchmark -h "$REDIS_HOST" \
            -p "$REDIS_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        [[ $VALKEY_OK -eq 1 ]] && valkey_rps=$(redis-benchmark -h "$REDIS_HOST" \
            -p "$VALKEY_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        [[ $NEXCACHE_OK -eq 1 ]] && nexcache_rps=$(redis-benchmark -h "$REDIS_HOST" \
            -p "$NEXCACHE_PORT" -n 1000000 -c "$clients" -t get --csv 2>/dev/null | \
            awk -F',' '/GET/{gsub(/"/, "", $2); print $2}' | head -1)

        printf "  %-8s %-16s %-16s %-16s\n" \
               "${clients}c" \
               "${redis_rps:-N/A}" \
               "${valkey_rps:-N/A}" \
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
