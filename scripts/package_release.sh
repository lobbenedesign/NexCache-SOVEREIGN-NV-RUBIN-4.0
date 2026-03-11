#!/bin/bash
# NexCache Release Packager v1.0
# ============================================================
# Questo script separa i file Core (Open Source) dai moduli
# Enterprise (Proprietari) per preparare la release GitHub.

set -e

SOURCE_DIR="./nexcache"
OUTPUT_DIR="./release_bundle"
CE_DIR="$OUTPUT_DIR/nexcache-ce"
EE_DIR="$OUTPUT_DIR/nexcache-ee"

echo "📦 Avvio packaging NexCache..."

# 1. Pulisci directory precedente
rm -rf "$OUTPUT_DIR"
mkdir -p "$CE_DIR" "$EE_DIR"

# 2. Copia struttura base in CE (Community Edition)
# Escludiamo esplicitamente i moduli Enterprise
echo "  > Creazione NexCache CE (Open Source)..."
cp -r "$SOURCE_DIR/"* "$CE_DIR/"

# Rimuovi moduli EE dal bundle CE
ENTERPRISE_MODULES=(
    "src/consensus/raft.c"
    "src/consensus/raft.h"
    "src/rdma.c"
    "src/rdma.h"
    "src/wasm/runtime.c"
    "src/wasm/runtime.h"
    "src/security/pqcrypto.c"
    "src/security/pqcrypto.h"
    "src/ai/semantic.c"
    "src/ai/semantic.h"
    "src/observability/anomaly.c"
    "src/observability/anomaly.h"
    "src/cloud_tier"
)

for mod in "${ENTERPRISE_MODULES[@]}"; do
    rm -rf "$CE_DIR/$mod"
done

# 3. Creazione EE (Enterprise Edition)
echo "  > Creazione NexCache EE (Proprietary)..."
cp -r "$SOURCE_DIR/"* "$EE_DIR/"

# 4. Generazione README specifici
cat <<EOF > "$CE_DIR/README_RELEASE.md"
# NexCache Community Edition (CE)
Questa è la versione Open Source di NexCache. 
Include l'engine Narrow-Waist, NexDashTable e il supporto DPDK/io_uring.
EOF

cat <<EOF > "$EE_DIR/README_RELEASE.md"
# NexCache Enterprise Edition (EE)
Questa versione include i moduli critici: Raft Consensus, RDMA Replication, WASM Runtime e Cloud Tiering.
EOF

echo "✅ Packaging completato in $OUTPUT_DIR"
echo "   - $CE_DIR: Pronto per GitHub"
echo "   - $EE_DIR: Mantieni PRIVATO"
