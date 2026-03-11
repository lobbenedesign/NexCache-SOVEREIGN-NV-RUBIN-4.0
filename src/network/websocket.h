/* NexCache WebSocket Nativo — MODULO 8
 * ============================================================
 * Supporto WebSocket nativo integrato nel server NexCache.
 * Elimina la necessità di middleware esterni (Socket.io, ecc.).
 *
 * Funzionamento:
 *   1. Il client si connette a ws://nexcache:6379/ws
 *   2. NexCache rileva automaticamente se è una connessione RESP o WS
 *   3. Se WebSocket: esegue l'handshake RFC 6455
 *   4. Post-handshake: i comandi arrivano in formato JSON
 *   5. Le risposte escono in formato JSON invece che RESP
 *   6. Pub/Sub funziona con push JSON nativo
 *
 * Formato messaggi WebSocket:
 *   Request:  { "cmd": "SET", "args": ["mykey", "value"], "id": "req-123" }
 *   Response: { "ok": true, "result": "OK", "id": "req-123" }
 *   PubSub:   { "type": "message", "channel": "news", "data": "..." }
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_WEBSOCKET_H
#define NEXCACHE_WEBSOCKET_H

#include <stdint.h>
#include <stddef.h>

/* ── Costanti WebSocket (RFC 6455) ──────────────────────────── */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAGIC WS_GUID
#define WS_VERSION "13"

/* Opcode WebSocket */
#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT 0x1
#define WS_OP_BINARY 0x2
#define WS_OP_CLOSE 0x8
#define WS_OP_PING 0x9
#define WS_OP_PONG 0xA

/* Dimensione massima frame */
#define WS_MAX_FRAME_SIZE (64 * 1024 * 1024) /* 64MB */
#define WS_MAX_PAYLOAD (16 * 1024 * 1024)    /* 16MB payload utile */

/* Buffer handshake */
#define WS_HANDSHAKE_BUF 4096

/* ── Stato connessione WebSocket ────────────────────────────── */
typedef enum WsState {
    WS_STATE_CONNECTING = 0, /* Connessione TCP in corso */
    WS_STATE_HANDSHAKE = 1,  /* HTTP upgrade in corso */
    WS_STATE_OPEN = 2,       /* WebSocket aperta */
    WS_STATE_CLOSING = 3,    /* Close handshake */
    WS_STATE_CLOSED = 4,     /* Connessione chiusa */
} WsState;

/* ── Frame WebSocket parsato ─────────────────────────────────── */
typedef struct WsFrame {
    uint8_t fin;            /* Final fragment */
    uint8_t opcode;         /* Tipo frame */
    uint8_t masked;         /* Frame maskato (da client: sempre 1) */
    uint8_t masking_key[4]; /* Chiave di mascheramento */
    uint64_t payload_len;   /* Lunghezza payload */
    uint8_t *payload;       /* Dati (già de-mascherati) */
    size_t payload_cap;     /* Capacità buffer */
} WsFrame;

/* ── Connessione WebSocket ───────────────────────────────────── */
typedef struct WsConn {
    int fd; /* File descriptor TCP */
    WsState state;
    uint64_t client_id; /* ID univoco client */

    /* Buffer I/O */
    uint8_t *read_buf;
    size_t read_len;
    size_t read_cap;

    uint8_t *write_buf;
    size_t write_len;
    size_t write_cap;

    /* Frame frame corrente in assembly */
    WsFrame current_frame;

    /* Canali Pub/Sub sottoscritti */
    char **subscribed_channels;
    int sub_count;
    int sub_cap;

    /* Stats */
    uint64_t frames_received;
    uint64_t frames_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t connected_at_us;
} WsConn;

/* ── API pubblica ───────────────────────────────────────────── */

/**
 * ws_detect_upgrade - Verifica se una richiesta HTTP è un WebSocket upgrade.
 * @data:   Dati ricevuti dal client
 * @len:    Lunghezza dati
 *
 * Returns: 1 se è un WebSocket upgrade, 0 altrimenti.
 */
int ws_detect_upgrade(const uint8_t *data, size_t len);

/**
 * ws_handshake - Esegui l'handshake WebSocket (RFC 6455).
 * @conn:   Connessione WebSocket
 * @request: Request HTTP ricevuta
 * @req_len: Lunghezza request
 *
 * Returns: 0 su successo, -1 su errore.
 */
int ws_handshake(WsConn *conn, const char *request, size_t req_len);

/**
 * ws_conn_create - Crea una nuova connessione WebSocket.
 */
WsConn *ws_conn_create(int fd, uint64_t client_id);

/**
 * ws_conn_destroy - Libera una connessione WebSocket.
 */
void ws_conn_destroy(WsConn *conn);

/**
 * ws_read_frame - Legge e parsifica un frame WebSocket dall'fd.
 * @conn:  Connessione
 * @frame: Output frame parsato
 *
 * Returns: 1 = frame completo letto, 0 = dati incompleti, -1 = errore.
 */
int ws_read_frame(WsConn *conn, WsFrame *frame);

/**
 * ws_send_text - Invia un frame di testo WebSocket.
 * @conn:    Connessione
 * @text:    Testo UTF-8 da inviare
 * @text_len: Lunghezza
 *
 * Returns: 0 su successo, -1 su errore.
 */
int ws_send_text(WsConn *conn, const char *text, size_t text_len);

/**
 * ws_send_binary - Invia un frame binario WebSocket.
 */
int ws_send_binary(WsConn *conn, const uint8_t *data, size_t data_len);

/**
 * ws_send_ping - Invia un ping per keepalive.
 */
int ws_send_ping(WsConn *conn);

/**
 * ws_send_close - Invia un frame di chiusura.
 * @code:   Codice di chiusura (1000 = normale)
 * @reason: Messaggio human-readable
 */
int ws_send_close(WsConn *conn, uint16_t code, const char *reason);

/**
 * ws_process_command - Processa un comando JSON ricevuto via WebSocket.
 * Traduce da JSON a comando NexCache interno.
 *
 * @conn:    Connessione WebSocket
 * @json:    Payload JSON del frame
 * @json_len: Lunghezza
 *
 * Returns: 0 su successo, -1 su errore.
 */
int ws_process_command(WsConn *conn, const char *json, size_t json_len);

/**
 * ws_send_response - Serializza una risposta NexCache in JSON e invia.
 * @conn:    Connessione WebSocket
 * @req_id:  ID request originale (per correlazione)
 * @result:  Risultato del comando
 * @error:   NULL se successo, messaggio di errore altrimenti
 */
int ws_send_response(WsConn *conn,
                     const char *req_id,
                     const char *result,
                     const char *error);

/**
 * ws_broadcast_pubsub - Invia un messaggio Pub/Sub a tutti i subscriber.
 * @channel:  Nome del canale
 * @message:  Messaggio da inviare
 * @msg_len:  Lunghezza messaggio
 */
int ws_broadcast_pubsub(const char *channel,
                        const char *message,
                        size_t msg_len);

/* ── Utility ──────────────────────────────────────────────── */

/**
 * ws_compute_accept_key - Calcola la Sec-WebSocket-Accept key.
 * @client_key: Valore di Sec-WebSocket-Key dal client
 * @out_key:    Buffer output (almeno 29 byte per base64)
 */
void ws_compute_accept_key(const char *client_key, char *out_key);

/**
 * ws_mask_payload - Applica/rimuovi il masking WebSocket.
 * (XOR con la masking key — stessa operazione in entrambe le direzioni)
 */
void ws_mask_payload(uint8_t *data, size_t len, const uint8_t mask[4]);

#endif /* NEXCACHE_WEBSOCKET_H */
