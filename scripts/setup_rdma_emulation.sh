#!/bin/bash
# NexCache — RDMA Emulation Setup via Soft-RoCE (rdma_rxe)
# ============================================================
# Abilitazione e configurazione dell'emulazione RDMA over Ethernet
# su macchine di sviluppo prive di hardware InfiniBand o RoCE.
#
# Utilizzato per validare l'infrastruttura di Replica Zero-CPU (< 20us)
# su Linux prima del passaggio in produzione.
#
# Copyright (c) 2026 NexCache Project

set -e

RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
RESET="\033[0m"

echo -e "${YELLOW}====================================================${RESET}"
echo -e "${YELLOW}  🖧  NEXCACHE RDMA EMULATION SETUP (rdma_rxe)    ${RESET}"
echo -e "${YELLOW}====================================================${RESET}"

if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo -e "${RED}[X] L'emulazione rdma_rxe richiede un kernel Linux recente (Soft-RoCE).${RESET}"
    echo "    Per sviluppo locale su macOS o WSL2 (senza kernel ib-core), l'Auto-Fallback"
    echo "    Backend disabiliterà automaticamente temporaneamente le chiamate RDMA."
    exit 0
fi

# Controlla privilegi di root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}[X] Lo script deve essere eseguito come root per caricare i moduli del kernel.${RESET}"
    exit 1
fi

echo -e "\n${YELLOW}[1] Installazione dipendenze essenziali...${RESET}"
apt-get update && apt-get install -y rdmacm-utils ibverbs-utils iproute2 libibverbs-dev

echo -e "\n${YELLOW}[2] Caricamento modulo kernel rdma_rxe...${RESET}"
modprobe rdma_rxe
if lsmod | grep -q rdma_rxe; then
    echo -e "    ${GREEN}> Modulo rdma_rxe caricato con successo.${RESET}"
else
    echo -e "    ${RED}Errore: Impossibile caricare il modulo rdma_rxe.${RESET}"
    exit 1
fi

# Trova un'interfaccia ethernet di loopback o fisica attiva (es eth0)
INTERFACE=$(ip -o link show | awk -F': ' '{print $2}' | grep -v "lo" | head -n 1)
if [ -z "$INTERFACE" ]; then
    INTERFACE="lo"
fi

echo -e "\n${YELLOW}[3] Abilitazione Soft-RoCE sull'interfaccia $INTERFACE...${RESET}"

# Rimuovi precedente emulatore se esiste
rdma link delete rxe0 2>/dev/null || true

# Crea il nuovo link RDMA fittizio
rdma link add rxe0 type rxe netdev "$INTERFACE"

if rdma link show | grep -q rxe0; then
    echo -e "    ${GREEN}> Dispositivo fittizio RDMA (rxe0) associato a $INTERFACE.${RESET}"
else
    echo -e "    ${RED}Errore: Fallita la configurazione di rxe0.${RESET}"
    exit 1
fi

echo -e "\n${YELLOW}[4] Verifica Hardware ed Emulazione...${RESET}"
ibv_devinfo -d rxe0 | grep "hca_id\|rxe"

echo -e "\n${GREEN}✅ SETUP COMPLETATO. Ambito di produzione simulato.${RESET}"
echo -e "   È ora possibile eseguire ./nexcache-server abilitando la modalità"
echo -e "   RDMA Replication per testare la sincronizzazione della DashTable v5."
echo -e "--------------------------------------------------------"
exit 0
