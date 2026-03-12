/* NexCache Protocol Auto-Detect — MODULO Network (v2.0)
 * ============================================================
 * Rileva automaticamente il protocollo della connessione in arrivo:
 *   RESP2/RESP3 — client NexCache/NexCache standard
 *   WebSocket   — ws:// o wss:// (upgrade da HTTP)
 *   GraphQL     — operazioni query/mutation/subscription
 *   gRPC        — /nexcache.NexCache/* HTTP/2
 *   HTTP REST   — /api/v1/*
 *
 * Il client NON deve specificare il protocollo — NexCache lo rileva
 * dai primi byte della connessione (detection window: 16 bytes max).
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_PROTOCOL_DETECT_H
#define NEXCACHE_PROTOCOL_DETECT_H

#include <stdint.h>
#include <stddef.h>

/* ── Protocolli supportati ──────────────────────────────────── */
typedef enum NexProtocol {
    NEX_PROTO_UNKNOWN = 0,   /* Non ancora determinato */
    NEX_PROTO_RESP2 = 1,     /* NexCache RESP2 */
    NEX_PROTO_RESP3 = 2,     /* NexCache RESP3 (HELLO 3) */
    NEX_PROTO_WEBSOCKET = 3, /* WebSocket RFC 6455 */
    NEX_PROTO_GRAPHQL = 4,   /* GraphQL over WebSocket */
    NEX_PROTO_GRPC = 5,      /* gRPC / HTTP2 */
    NEX_PROTO_HTTP_REST = 6, /* HTTP/1.1 REST API */
} NexProtocol;

/* ── Risultato del detection ────────────────────────────────── */
typedef struct ProtoDetectResult {
    NexProtocol protocol;
    int need_more_data; /* 1 = serve più dati per decidere */
    int is_tls;         /* 1 = connessione TLS */
    char version[16];   /* Versione protocollo rilevata */
} ProtoDetectResult;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * protocol_detect - Rileva il protocollo dai primi byte.
 *
 * Firma dei protocolli:
 *   RESP2/3:    '+', '-', ':', '$', '*', '_', ',', '#', '!', '=', '>', '|', '~', '%'
 *   WebSocket:  "GET " poi "Upgrade: websocket" nell'HTTP header
 *   gRPC:       "\x50\x52\x49\x20" (PRI * HTTP/2.0)
 *   HTTP REST:  "GET " o "POST " senza Upgrade header
 *   TLS:        0x16 (record type handshake) come primo byte
 *
 * @data:    Primi bytes della connessione
 * @len:     Numero di bytes disponibili
 * @result:  Output risultato detection
 *
 * Returns: 0 = identificato, 1 = servono più dati, -1 = protocollo invalido.
 */
int protocol_detect(const uint8_t *data, size_t len, ProtoDetectResult *result);

/**
 * protocol_name - Nome human-readable del protocollo.
 */
const char *protocol_name(NexProtocol proto);

#endif /* NEXCACHE_PROTOCOL_DETECT_H */
