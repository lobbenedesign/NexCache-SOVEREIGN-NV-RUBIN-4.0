/* NexCache Post-Quantum Security — MODULO (Enterprise)
 * ============================================================
 * Stack sicurezza enterprise con algoritmi post-quantum ready.
 * Usa CRYSTALS-Kyber per key encapsulation e
 * CRYSTALS-Dilithium per digital signatures.
 * BLAKE3 per hashing crittografico ultra-veloce.
 *
 * Copyright (c) 2026 NexCache Project — BSD License
 */

#ifndef NEXCACHE_SECURITY_H
#define NEXCACHE_SECURITY_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ── Costanti algoritmi PQ ─────────────────────────────────── */
#define DILITHIUM3_PK_BYTES 1952
#define DILITHIUM3_SK_BYTES 4000
#define DILITHIUM3_SIG_BYTES 3293
#define KYBER768_PK_BYTES 1184
#define KYBER768_SK_BYTES 2400
#define KYBER768_CT_BYTES 1088
#define KYBER768_SS_BYTES 32
#define BLAKE3_OUT_BYTES 32
#define PQ_TOKEN_BYTES 64
#define PQ_SIGNATURE_BYTES DILITHIUM3_SIG_BYTES

/* ── Algoritmi post-quantum supportati ──────────────────────── */
typedef enum PQAlgorithm {
    PQ_NONE = 0,
    PQ_KYBER_512 = 1,
    PQ_KYBER_768 = 2,
    PQ_KYBER_1024 = 3,
    PQ_HYBRID = 4,
} PQAlgorithm;

/* ── Configurazione sicurezza ───────────────────────────────── */
typedef struct SecurityConfig {
    int tls_required;
    int tls_version_min;
    PQAlgorithm pq_algorithm;
    int audit_log_enabled;
    char audit_log_path[512];
    int acl_enabled;
    int rate_limit_enabled;
    int max_req_per_sec;
} SecurityConfig;

/* ── Audit log entry ───────────────────────────────────────── */
typedef struct AuditEntry {
    uint64_t timestamp_us;
    uint64_t client_id;
    char client_ip[46];
    char command[128];
    char key[256];
    int success;
    char reason[256];
} AuditEntry;

/* ── API sicurezza classica ─────────────────────────────────── */
int security_init(const SecurityConfig *cfg);
void security_shutdown(void);
int audit_log_write(const AuditEntry *entry);
int audit_log_flush(void);
int acl_check(uint64_t client_id, const char *command, const char *key);
int acl_add_rule(const char *pattern, const char *permissions);
int ratelimit_check(uint64_t client_id);

/* ── API Post-Quantum ───────────────────────────────────────── */
int pq_crypto_init(void);
void pq_crypto_shutdown(void);
void pq_blake3_hash(const uint8_t *data, size_t len, uint8_t *out, size_t out_len);
uint64_t pq_hash_key(const char *key, size_t keylen);
int pq_generate_token(const char *namespace_id,
                      const char *secret_key,
                      uint8_t *token_out,
                      size_t token_out_len);
int pq_verify_token(const uint8_t *token, size_t token_len, const char *secret_key, uint64_t max_age_us);
int pq_sign_keygen(uint8_t *pub_key, size_t pub_len, uint8_t *priv_key, size_t priv_len);
int pq_sign(const uint8_t *msg, size_t msg_len, const uint8_t *priv_key, size_t priv_len, uint8_t *sig_out, size_t *sig_len);
int pq_verify(const uint8_t *msg, size_t msg_len, const uint8_t *sig, size_t sig_len, const uint8_t *pub_key, size_t pub_len);

#endif /* NEXCACHE_SECURITY_H */
