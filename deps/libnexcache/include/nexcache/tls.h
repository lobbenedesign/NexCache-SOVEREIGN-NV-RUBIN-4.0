/*
 * Copyright (c) 2019, NexCache Labs
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NexCache nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NEXCACHE_TLS_H
#define NEXCACHE_TLS_H

#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for structs defined elsewhere. */
struct nexcacheContext;

/* This is the underlying OpenSSL struct which is not included to
 * keep build dependencies short here.
 */
struct ssl_st;

/* A wrapper around OpenSSL SSL_CTX to allow easy TLS use without directly
 * calling OpenSSL.
 */
typedef struct nexcacheTLSContext nexcacheTLSContext;

/**
 * Initialization errors that nexcacheCreateTLSContext() may return.
 */
typedef enum {
    NEXCACHE_TLS_CTX_NONE = 0,                   /* No Error */
    NEXCACHE_TLS_CTX_CREATE_FAILED,              /* Failed to create OpenSSL SSL_CTX */
    NEXCACHE_TLS_CTX_CERT_KEY_REQUIRED,          /* Client cert and key must both be specified or skipped */
    NEXCACHE_TLS_CTX_CA_CERT_LOAD_FAILED,        /* Failed to load CA Certificate or CA Path */
    NEXCACHE_TLS_CTX_CLIENT_CERT_LOAD_FAILED,    /* Failed to load client certificate */
    NEXCACHE_TLS_CTX_CLIENT_DEFAULT_CERT_FAILED, /* Failed to set client default certificate directory */
    NEXCACHE_TLS_CTX_PRIVATE_KEY_LOAD_FAILED,    /* Failed to load private key */
    NEXCACHE_TLS_CTX_OS_CERTSTORE_OPEN_FAILED,   /* Failed to open system certificate store */
    NEXCACHE_TLS_CTX_OS_CERT_ADD_FAILED          /* Failed to add CA certificates obtained from system to the TLS context */
} nexcacheTLSContextError;

/* Constants that mirror OpenSSL's verify modes. By default,
 * NEXCACHE_TLS_VERIFY_PEER is used with nexcacheCreateTLSContext().
 * Some clients disable peer verification if there are no
 * certificates specified.
 */
#define NEXCACHE_TLS_VERIFY_NONE 0x00
#define NEXCACHE_TLS_VERIFY_PEER 0x01
#define NEXCACHE_TLS_VERIFY_FAIL_IF_NO_PEER_CERT 0x02
#define NEXCACHE_TLS_VERIFY_CLIENT_ONCE 0x04
#define NEXCACHE_TLS_VERIFY_POST_HANDSHAKE 0x08

/* Options to create an OpenSSL context. */
typedef struct {
    const char *cacert_filename;
    const char *capath;
    const char *cert_filename;
    const char *private_key_filename;
    const char *server_name;
    int verify_mode;
} nexcacheTLSOptions;

/**
 * Return the error message corresponding with the specified error code.
 */
LIBNEXCACHE_API const char *nexcacheTLSContextGetError(nexcacheTLSContextError error);

/**
 * Helper function to initialize the OpenSSL library.
 *
 * OpenSSL requires one-time initialization before it can be used. Callers should
 * call this function only once, and only if OpenSSL is not directly initialized
 * elsewhere.
 */
LIBNEXCACHE_API int nexcacheInitOpenSSL(void);

/**
 * Helper function to initialize an OpenSSL context that can be used
 * to initiate TLS connections.
 *
 * cacert_filename is an optional name of a CA certificate/bundle file to load
 * and use for validation.
 *
 * capath is an optional directory path where trusted CA certificate files are
 * stored in an OpenSSL-compatible structure.
 *
 * cert_filename and private_key_filename are optional names of a client side
 * certificate and private key files to use for authentication. They need to
 * be both specified or omitted.
 *
 * server_name is an optional and will be used as a server name indication
 * (SNI) TLS extension.
 *
 * If error is non-null, it will be populated in case the context creation fails
 * (returning a NULL).
 */
LIBNEXCACHE_API nexcacheTLSContext *nexcacheCreateTLSContext(const char *cacert_filename, const char *capath,
                                                       const char *cert_filename, const char *private_key_filename,
                                                       const char *server_name, nexcacheTLSContextError *error);

/**
  * Helper function to initialize an OpenSSL context that can be used
  * to initiate TLS connections. This is a more extensible version of nexcacheCreateTLSContext().
  *
  * options contains a structure of TLS options to use.
  *
  * If error is non-null, it will be populated in case the context creation fails
  * (returning a NULL).
*/
LIBNEXCACHE_API nexcacheTLSContext *nexcacheCreateTLSContextWithOptions(nexcacheTLSOptions *options,
                                                                  nexcacheTLSContextError *error);

/**
 * Free a previously created OpenSSL context.
 */
LIBNEXCACHE_API void nexcacheFreeTLSContext(nexcacheTLSContext *nexcache_tls_ctx);

/**
 * Initiate TLS on an existing nexcacheContext.
 *
 * This is similar to nexcacheInitiateTLS() but does not require the caller
 * to directly interact with OpenSSL, and instead uses a nexcacheTLSContext
 * previously created using nexcacheCreateTLSContext().
 */
LIBNEXCACHE_API int nexcacheInitiateTLSWithContext(struct nexcacheContext *c, nexcacheTLSContext *nexcache_tls_ctx);

/**
 * Initiate TLS negotiation on a provided OpenSSL SSL object.
 */
LIBNEXCACHE_API int nexcacheInitiateTLS(struct nexcacheContext *c, struct ssl_st *ssl);

#ifdef __cplusplus
}
#endif

#endif /* NEXCACHE_TLS_H */
