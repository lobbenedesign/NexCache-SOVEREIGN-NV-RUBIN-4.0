/* NexCache Protocol Auto-Detect — Implementazione
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#include "protocol_detect.h"
#include <string.h>
#include <stdio.h>

/* ── Nome protocollo ────────────────────────────────────────── */
const char *protocol_name(NexProtocol proto) {
    switch (proto) {
    case NEX_PROTO_RESP2: return "RESP2";
    case NEX_PROTO_RESP3: return "RESP3";
    case NEX_PROTO_WEBSOCKET: return "WebSocket";
    case NEX_PROTO_GRAPHQL: return "GraphQL";
    case NEX_PROTO_GRPC: return "gRPC";
    case NEX_PROTO_HTTP_REST: return "HTTP-REST";
    default: return "Unknown";
    }
}

/* ── Helpers ────────────────────────────────────────────────── */
static int starts_with(const uint8_t *data, size_t len, const char *prefix) {
    size_t plen = strlen(prefix);
    if (len < plen) return 0;
    return memcmp(data, prefix, plen) == 0;
}

static int contains_header(const uint8_t *data, size_t len, const char *header) {
    size_t hlen = strlen(header);
    if (len < hlen) return 0;
    /* Ricerca case-insensitive semplificata */
    for (size_t i = 0; i + hlen <= len; i++) {
        int match = 1;
        for (size_t j = 0; j < hlen; j++) {
            char a = (char)data[i + j];
            char b = header[j];
            if (a >= 'A' && a <= 'Z') a += 32; /* tolower */
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/* ── Cuore del detection ────────────────────────────────────── */
int protocol_detect(const uint8_t *data, size_t len, ProtoDetectResult *result) {
    if (!data || !result) return -1;

    result->protocol = NEX_PROTO_UNKNOWN;
    result->need_more_data = 0;
    result->is_tls = 0;
    result->version[0] = '\0';

    if (len == 0) {
        result->need_more_data = 1;
        return 1;
    }

    /* ── TLS detection (primo byte = 0x16 = TLS Handshake record) ── */
    if (data[0] == 0x16 && len >= 3) {
        result->is_tls = 1;
        /* Dopo TLS handshake, il protocollo interno verrà rilevato */
        result->need_more_data = 1;
        return 1;
    }

    /* ── gRPC (HTTP/2 PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n) ── */
    if (starts_with(data, len, "PRI * HTTP/2.0")) {
        result->protocol = NEX_PROTO_GRPC;
        snprintf(result->version, sizeof(result->version), "HTTP/2");
        return 0;
    }

    /* ── HTTP request (GET/POST/PUT/DELETE/OPTIONS/HEAD) ── */
    int is_http = starts_with(data, len, "GET ") ||
                  starts_with(data, len, "POST ") ||
                  starts_with(data, len, "PUT ") ||
                  starts_with(data, len, "DELETE ") ||
                  starts_with(data, len, "OPTIONS ") ||
                  starts_with(data, len, "HEAD ");

    if (is_http) {
        /* Per distinguere WS da REST dobbiamo leggere gli headers HTTP.
         * Se abbiamo almeno il terminatore headers (\r\n\r\n) o
         * più di 16 byte, possiamo già decidere. */
        int has_headers_end =
            (len >= 4 &&
             memmem(data, len, "\r\n\r\n", 4) != NULL);

        if (!has_headers_end && len < 16) {
            result->need_more_data = 1;
            return 1;
        }

        /* WebSocket upgrade */
        if (contains_header(data, len, "upgrade: websocket") ||
            contains_header(data, len, "upgrade:websocket")) {
            /* GraphQL over WebSocket */
            if (contains_header(data, len, "/graphql") ||
                contains_header(data, len, "graphql-ws") ||
                contains_header(data, len, "graphql-transport-ws")) {
                result->protocol = NEX_PROTO_GRAPHQL;
                snprintf(result->version, sizeof(result->version), "graphql-ws");
            } else {
                result->protocol = NEX_PROTO_WEBSOCKET;
                snprintf(result->version, sizeof(result->version), "RFC6455");
            }
            return 0;
        }

        /* HTTP REST puro */
        result->protocol = NEX_PROTO_HTTP_REST;
        snprintf(result->version, sizeof(result->version), "HTTP/1.1");
        return 0;
    }

    /* ── RESP2/RESP3 detection ── */
    /*
     * RESP2: comandi inline o prefissati con tipo:
     *   '*' — Array
     *   '$' — Bulk String
     *   '+' — Simple String (risposta server, non comando client usuale)
     *   '-' — Error
     *   ':' — Integer
     *
     * RESP3 aggiunge:
     *   '_' — Null
     *   ',' — Double
     *   '#' — Boolean
     *   '!' — Blob Error
     *   '=' — Verbatim String
     *   '>' — Push
     *   '|' — Attribute
     *   '~' — Set
     *   '%' — Map
     *
     * Il comando HELLO 3 inizia con: *2\r\n$5\r\nHELLO\r\n$1\r\n3
     * che è comunque un array RESP2, quindi inizio con RESP2 detection.
     */
    const char resp2_types[] = "*$+-:";
    const char resp3_extras[] = "_,#!=>|~%";

    if (data[0] != '\0') {
        int is_resp2 = (strchr(resp2_types, (char)data[0]) != NULL);
        int is_resp3_extra = (strchr(resp3_extras, (char)data[0]) != NULL);

        if (is_resp2 || is_resp3_extra) {
            /* Inline command check: il RESP2 inizia spesso con PING inline */
            /* Per ora classifichiamo come RESP2 — HELLO 3 lo può upgradare */
            result->protocol = is_resp3_extra ? NEX_PROTO_RESP3 : NEX_PROTO_RESP2;
            snprintf(result->version, sizeof(result->version),
                     is_resp3_extra ? "3" : "2");
            return 0;
        }

        /* Inline command: PING, SET, GET senza prefisso RESP */
        if (data[0] >= 'A' && data[0] <= 'Z') {
            result->protocol = NEX_PROTO_RESP2;
            snprintf(result->version, sizeof(result->version), "2-inline");
            return 0;
        }
    }

    /* Serve più dati */
    if (len < 16) {
        result->need_more_data = 1;
        return 1;
    }

    /* Protocollo non riconosciuto */
    return -1;
}
