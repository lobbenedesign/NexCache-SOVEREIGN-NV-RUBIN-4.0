#!/bin/bash
# benchmarks/run_all.sh
# NexCache vs NexCache vs NexCache vs Dragonfly — Benchmark Suite
# Riproducibile: qualsiasi cosa nel report può essere verificata rieseguendo questo script

set -e

NEXCACHE_PORT=6379
NEXCACHE_PORT=6380
NEXCACHE_PORT=6381
DRAGONFLY_PORT=6382

DURATION=60      # secondi per test
THREADS=16       # thread memtier
CLIENTS=50       # client per thread
REQUESTS=2000000 # per test nexcache-benchmark

echo "======================================================"
echo " NEXCACHE BENCHMARK SUITE"
echo " $(date)"
echo " Hardware: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2)"
echo " CPU cores: $(nproc)"
echo " RAM: $(free -h | grep Mem | awk '{print $2}')"
echo " Kernel: $(uname -r)"
echo "======================================================"

# Funzione benchmark singolo sistema
benchmark_system() {
  local name=$1
  local port=$2

  echo ""
  echo "--- $name (porta $port) ---"

  # Verifica che il sistema sia raggiungibile
  if ! nexcache-cli -p $port ping > /dev/null 2>&1; then
    echo "SKIP: $name non raggiungibile su porta $port"
    return
  fi

  echo "SET throughput:"
  memtier_benchmark -p $port -t $THREADS -c $CLIENTS \
    --ratio=1:0 --test-time=$DURATION \
    --print-percentiles=50,99,99.9 --hide-histogram 2>/dev/null

  echo "GET throughput:"
  memtier_benchmark -p $port -t $THREADS -c $CLIENTS \
    --ratio=0:1 --test-time=$DURATION \
    --print-percentiles=50,99,99.9 --hide-histogram 2>/dev/null

  echo "Memoria (50M chiavi 100 bytes):"
  nexcache-cli -p $port info memory | grep used_memory_human
}

# Prepara sistema
echo "Preparazione sistema..."
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  echo performance | sudo tee $cpu > /dev/null 2>&1 || true
done
sudo sysctl -w vm.overcommit_memory=1 > /dev/null || true
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled > /dev/null 2>&1 || true

# Latenza intrinseca
echo ""
echo "=== Latenza intrinseca sistema (deve essere < 500μs) ==="
nexcache-cli -p $NEXCACHE_PORT --intrinsic-latency 10 || echo "NexCache-cli intrinsic latency test skipped"

# Benchmark tutti i sistemi
echo ""
echo "=== TEST 1: THROUGHPUT MASSIMO ==="
benchmark_system "NexCache" $NEXCACHE_PORT
benchmark_system "NexCache 8.6" $NEXCACHE_PORT
benchmark_system "NexCache 9.0" $NEXCACHE_PORT
benchmark_system "Dragonfly" $DRAGONFLY_PORT

# Test scaling multi-core
echo ""
echo "=== TEST 2: SCALING MULTI-CORE ==="
echo "Connessioni | NexCache QPS | NexCache QPS | NexCache QPS"
for c in 1 10 50 100 200 500; do
  nc=$(memtier_benchmark -p $NEXCACHE_PORT -t 4 -c $c \
    --ratio=1:9 --test-time=10 -q 2>/dev/null | \
    grep Totals | awk '{print $2}' || echo "N/A")
  rc=$(memtier_benchmark -p $NEXCACHE_PORT -t 4 -c $c \
    --ratio=1:9 --test-time=10 -q 2>/dev/null | \
    grep Totals | awk '{print $2}' || echo "N/A")
  echo "$c connessioni: NexCache=$nc | NexCache=$rc"
done

# Test workload TTL-Heavy (Segcache)
echo ""
echo "=== TEST 4: WORKLOAD TTL-HEAVY ==="
memtier_benchmark -p $NEXCACHE_PORT -t $THREADS -c $CLIENTS \
  --ratio=1:9 --test-time=$DURATION \
  --key-expiry-range=30-300 \
  --print-percentiles=50,99,99.9 2>/dev/null || echo "Memtier benchmark non trovato/saltato"

echo ""
echo "======================================================"
echo " FINE BENCHMARK — $(date)"
echo "======================================================"
