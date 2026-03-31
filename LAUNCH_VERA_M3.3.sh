#!/bin/bash
# NEX-VERA M3.3: Launch & Benchmark Suite
# Optimized for NVIDIA Rubin / Apple Silicon (M1/M2/M3)

echo "🚀 [Vera M3.3] Starting Hyper-Performance Engine..."

# 1. Setup RDMA/C2C Environment (simulated in localized runs)
bash ./rdma_setup.sh

# 2. Build with GODMODE optimizations
echo "⚡ Building Vera M3.3 with SVE2, MPSC, and SVI..."
cd src && make clean && make -j$(nproc)
cd ..

# 3. Launch Server in Rubin Mode (176 Shards)
# Note: On MacOS, it will use 176 logical shards despite having 8-12 cores.
./src/nexcache-server --port 19003 --protected-mode no --save "" --appendonly no --io-threads 4 &
SERVER_PID=$!

sleep 2
echo "📊 Benchmarking Vera M3.3 (High-Throughput Mode)..."
memtier_benchmark --port 19003 --protocol=redis --threads=4 --clients=50 --ratio=1:10 --data-size=128 --requests=1000000

echo "🛑 Cleaning up..."
kill $SERVER_PID
echo "✅ Done. Check logs for SVI/SVE2 parsing efficiency."
