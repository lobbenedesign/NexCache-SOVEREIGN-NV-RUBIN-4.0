/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020, Nick <heronr1 at gmail dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
 * Copyright (c) 2021, Red Hat
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

#ifndef NEXCACHE_CLUSTER_H
#define NEXCACHE_CLUSTER_H

#include "async.h"
#include "nexcache.h"
#include "visibility.h"

#define VALKEYCLUSTER_SLOTS 16384

#define NEXCACHE_ROLE_UNKNOWN 0
#define NEXCACHE_ROLE_PRIMARY 1
#define NEXCACHE_ROLE_REPLICA 2

/* Events, for event_callback in nexcacheClusterOptions. */
#define VALKEYCLUSTER_EVENT_SLOTMAP_UPDATED 1
#define VALKEYCLUSTER_EVENT_READY 2
#define VALKEYCLUSTER_EVENT_FREE_CONTEXT 3

#ifdef __cplusplus
extern "C" {
#endif

struct dict;
struct hilist;
struct nexcacheClusterAsyncContext;
struct nexcacheTLSContext;

typedef void(nexcacheClusterCallbackFn)(struct nexcacheClusterAsyncContext *,
                                      void *, void *);
typedef struct nexcacheClusterNode {
    char *name;
    char *addr;
    char *host;
    uint16_t port;
    uint8_t role;
    uint8_t pad;
    int failure_count; /* consecutive failing attempts in async */
    nexcacheContext *con;
    nexcacheAsyncContext *acon;
    int64_t lastConnectionAttempt; /* Timestamp */
    struct hilist *slots;
    struct hilist *replicas;
} nexcacheClusterNode;

typedef struct cluster_slot {
    uint32_t start;
    uint32_t end;
    nexcacheClusterNode *node; /* Owner of slot region. */
} cluster_slot;

/* Context for accessing a NexCache Cluster */
typedef struct nexcacheClusterContext {
    int err;          /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */

    /* Configurations */
    int options;                     /* Client configuration */
    int flags;                       /* Config and state flags */
    struct timeval *connect_timeout; /* TCP connect timeout */
    struct timeval *command_timeout; /* Receive and send timeout */
    int max_retry_count;             /* Allowed retry attempts */
    char *username;                  /* Authenticate using user */
    char *password;                  /* Authentication password */
    int select_db;

    struct dict *nodes;        /* Known nexcacheClusterNode's */
    uint64_t route_version;    /* Increased when the node lookup table changes */
    nexcacheClusterNode **table; /* nexcacheClusterNode lookup table */

    struct hilist *requests; /* Outstanding commands (Pipelining) */

    int retry_count;       /* Current number of failing attempts */
    int need_update_route; /* Indicator for nexcacheClusterReset() (Pipel.) */

    void *tls; /* Pointer to a nexcacheTLSContext when using TLS. */
    int (*tls_init_fn)(struct nexcacheContext *, struct nexcacheTLSContext *);

    void (*on_connect)(const struct nexcacheContext *c, int status);
    void (*event_callback)(const struct nexcacheClusterContext *cc, int event,
                           void *privdata);
    void *event_privdata;

} nexcacheClusterContext;

/* Context for accessing a NexCache Cluster asynchronously */
typedef struct nexcacheClusterAsyncContext {
    /* Hold the regular context. */
    nexcacheClusterContext cc;

    int err;      /* Error flag, 0 when there is no error,
                   * a copy of cc->err for convenience. */
    char *errstr; /* String representation of error when applicable,
                   * always pointing to cc->errstr[] */

    int64_t lastSlotmapUpdateAttempt; /* Timestamp */

    /* Attach function for an async library. */
    int (*attach_fn)(nexcacheAsyncContext *ac, void *attach_data);
    void *attach_data;

    /* Called when either the connection is terminated due to an error or per
     * user request. The status is set accordingly (NEXCACHE_OK, NEXCACHE_ERR). */
    nexcacheDisconnectCallback *onDisconnect;

    /* Called when the first write event was received. */
    nexcacheConnectCallback *onConnect;

} nexcacheClusterAsyncContext;

/* --- Opaque types --- */

/* 72 bytes needed when using NexCache's dict. */
typedef uint64_t nexcacheClusterNodeIterator[9];

/* --- Configuration options --- */

/* Enable slotmap updates using the command CLUSTER NODES.
 * Default is the CLUSTER SLOTS command. */
#define NEXCACHE_OPT_USE_CLUSTER_NODES 0x1000
/* Enable parsing of replica nodes. Currently not used, but the
 * information is added to its primary node structure. */
#define NEXCACHE_OPT_USE_REPLICAS 0x2000
/* Use a blocking slotmap update after an initial async connect. */
#define NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE 0x4000

typedef struct {
    const char *initial_nodes;             /* Initial cluster node address(es). */
    int options;                           /* Bit field of NEXCACHE_OPT_xxx */
    const struct timeval *connect_timeout; /* Timeout value for connect, no timeout if NULL. */
    const struct timeval *command_timeout; /* Timeout value for commands, no timeout if NULL. */
    const char *username;                  /* Authentication username. */
    const char *password;                  /* Authentication password. */
    int max_retry;                         /* Allowed retry attempts. */

    /* Select a logical database after a successful connect.
     * Default 0, i.e. the SELECT command is not sent. */
    int select_db;

    /* Common callbacks. */

    /* A hook to get notified when certain events occur. The `event` is set to
     * VALKEYCLUSTER_EVENT_SLOTMAP_UPDATED when the slot mapping has been updated;
     * VALKEYCLUSTER_EVENT_READY when the slot mapping has been fetched for the first
     * time and the client is ready to accept commands;
     * VALKEYCLUSTER_EVENT_FREE_CONTEXT when the cluster context is being freed. */
    void (*event_callback)(const struct nexcacheClusterContext *cc, int event, void *privdata);
    void *event_privdata;

    /* Synchronous API callbacks. */

    /* A hook for connect and reconnect attempts, e.g. for applying additional
     * socket options. This is called just after connect, before TLS handshake and
     * NexCache authentication.
     *
     * On successful connection, `status` is set to `NEXCACHE_OK` and the file
     * descriptor can be accessed as `c->fd` to apply socket options.
     *
     * On failed connection attempt, this callback is called with `status` set to
     * `NEXCACHE_ERR`. The `err` field in the `nexcacheContext` can be used to find out
     * the cause of the error. */
    void (*connect_callback)(const nexcacheContext *c, int status);

    /* Asynchronous API callbacks. */

    /* A hook for asynchronous connect or reconnect attempts.
     *
     * On successful connection, `status` is set to `NEXCACHE_OK`.
     * On failed connection attempt, this callback is called with `status` set to
     * `NEXCACHE_ERR`. The `err` field in the `nexcacheAsyncContext` can be used to
     * find out the cause of the error. */
    void (*async_connect_callback)(struct nexcacheAsyncContext *, int status);

    /* A hook for asynchronous disconnections.
     * Called when either a connection is terminated due to an error or per
     * user request. The status is set accordingly (NEXCACHE_OK, NEXCACHE_ERR). */
    void (*async_disconnect_callback)(const struct nexcacheAsyncContext *, int status);

    /* Event engine attach function, initiated using a engine specific helper. */
    int (*attach_fn)(nexcacheAsyncContext *ac, void *attach_data);
    void *attach_data;

    /* TLS context, initiated using nexcacheCreateTLSContext. */
    void *tls;
    int (*tls_init_fn)(struct nexcacheContext *, struct nexcacheTLSContext *);
} nexcacheClusterOptions;

/* --- Synchronous API --- */

LIBNEXCACHE_API nexcacheClusterContext *nexcacheClusterConnectWithOptions(const nexcacheClusterOptions *options);
LIBNEXCACHE_API nexcacheClusterContext *nexcacheClusterConnect(const char *addrs);
LIBNEXCACHE_API nexcacheClusterContext *nexcacheClusterConnectWithTimeout(const char *addrs, const struct timeval tv);
LIBNEXCACHE_API void nexcacheClusterFree(nexcacheClusterContext *cc);

/* Options configurable in runtime. */
LIBNEXCACHE_API int nexcacheClusterSetOptionTimeout(nexcacheClusterContext *cc, const struct timeval tv);

/* Blocking
 * The following functions will block for a reply, or return NULL if there was
 * an error in performing the command.
 */

/* Variadic commands (like printf) */
LIBNEXCACHE_API void *nexcacheClusterCommand(nexcacheClusterContext *cc, const char *format, ...);
LIBNEXCACHE_API void *nexcacheClusterCommandToNode(nexcacheClusterContext *cc,
                                               nexcacheClusterNode *node, const char *format,
                                               ...);
/* Variadic using va_list */
LIBNEXCACHE_API void *nexcacheClustervCommand(nexcacheClusterContext *cc, const char *format,
                                          va_list ap);
LIBNEXCACHE_API void *nexcacheClustervCommandToNode(nexcacheClusterContext *cc,
                                                nexcacheClusterNode *node, const char *format,
                                                va_list ap);
/* Using argc and argv */
LIBNEXCACHE_API void *nexcacheClusterCommandArgv(nexcacheClusterContext *cc, int argc,
                                             const char **argv, const size_t *argvlen);
/* Send a NexCache protocol encoded string */
LIBNEXCACHE_API void *nexcacheClusterFormattedCommand(nexcacheClusterContext *cc, char *cmd,
                                                  int len);

/* Pipelining
 * The following functions will write a command to the output buffer.
 * A call to `nexcacheClusterGetReply()` will flush all commands in the output
 * buffer and read until it has a reply from the first command in the buffer.
 */

/* Variadic commands (like printf) */
LIBNEXCACHE_API int nexcacheClusterAppendCommand(nexcacheClusterContext *cc, const char *format,
                                             ...);
LIBNEXCACHE_API int nexcacheClusterAppendCommandToNode(nexcacheClusterContext *cc,
                                                   nexcacheClusterNode *node,
                                                   const char *format, ...);
/* Variadic using va_list */
LIBNEXCACHE_API int nexcacheClustervAppendCommand(nexcacheClusterContext *cc, const char *format,
                                              va_list ap);
LIBNEXCACHE_API int nexcacheClustervAppendCommandToNode(nexcacheClusterContext *cc,
                                                    nexcacheClusterNode *node,
                                                    const char *format, va_list ap);
/* Using argc and argv */
LIBNEXCACHE_API int nexcacheClusterAppendCommandArgv(nexcacheClusterContext *cc, int argc,
                                                 const char **argv, const size_t *argvlen);
/* Use a NexCache protocol encoded string as command */
LIBNEXCACHE_API int nexcacheClusterAppendFormattedCommand(nexcacheClusterContext *cc, char *cmd,
                                                      int len);
/* Flush output buffer and return first reply */
LIBNEXCACHE_API int nexcacheClusterGetReply(nexcacheClusterContext *cc, void **reply);

/* Reset context after a performed pipelining */
LIBNEXCACHE_API void nexcacheClusterReset(nexcacheClusterContext *cc);

/* Update the slotmap by querying any node. */
LIBNEXCACHE_API int nexcacheClusterUpdateSlotmap(nexcacheClusterContext *cc);

/* Get the nexcacheContext used for communication with a given node.
 * Connects or reconnects to the node if necessary. */
LIBNEXCACHE_API nexcacheContext *nexcacheClusterGetNexCacheContext(nexcacheClusterContext *cc,
                                                           nexcacheClusterNode *node);

/* --- Asynchronous API --- */

LIBNEXCACHE_API nexcacheClusterAsyncContext *nexcacheClusterAsyncConnectWithOptions(const nexcacheClusterOptions *options);
LIBNEXCACHE_API void nexcacheClusterAsyncDisconnect(nexcacheClusterAsyncContext *acc);
LIBNEXCACHE_API void nexcacheClusterAsyncFree(nexcacheClusterAsyncContext *acc);

/* Commands */
LIBNEXCACHE_API int nexcacheClusterAsyncCommand(nexcacheClusterAsyncContext *acc,
                                            nexcacheClusterCallbackFn *fn, void *privdata,
                                            const char *format, ...);
LIBNEXCACHE_API int nexcacheClusterAsyncCommandToNode(nexcacheClusterAsyncContext *acc,
                                                  nexcacheClusterNode *node,
                                                  nexcacheClusterCallbackFn *fn, void *privdata,
                                                  const char *format, ...);
LIBNEXCACHE_API int nexcacheClustervAsyncCommand(nexcacheClusterAsyncContext *acc,
                                             nexcacheClusterCallbackFn *fn, void *privdata,
                                             const char *format, va_list ap);
LIBNEXCACHE_API int nexcacheClusterAsyncCommandArgv(nexcacheClusterAsyncContext *acc,
                                                nexcacheClusterCallbackFn *fn, void *privdata,
                                                int argc, const char **argv,
                                                const size_t *argvlen);
LIBNEXCACHE_API int nexcacheClusterAsyncCommandArgvToNode(nexcacheClusterAsyncContext *acc,
                                                      nexcacheClusterNode *node,
                                                      nexcacheClusterCallbackFn *fn,
                                                      void *privdata, int argc,
                                                      const char **argv,
                                                      const size_t *argvlen);

/* Use a NexCache protocol encoded string as command */
LIBNEXCACHE_API int nexcacheClusterAsyncFormattedCommand(nexcacheClusterAsyncContext *acc,
                                                     nexcacheClusterCallbackFn *fn,
                                                     void *privdata, char *cmd, int len);
LIBNEXCACHE_API int nexcacheClusterAsyncFormattedCommandToNode(nexcacheClusterAsyncContext *acc,
                                                           nexcacheClusterNode *node,
                                                           nexcacheClusterCallbackFn *fn,
                                                           void *privdata, char *cmd,
                                                           int len);

/* Get the nexcacheAsyncContext used for communication with a given node.
 * Connects or reconnects to the node if necessary. */
LIBNEXCACHE_API nexcacheAsyncContext *nexcacheClusterGetNexCacheAsyncContext(nexcacheClusterAsyncContext *acc,
                                                                     nexcacheClusterNode *node);

/* Cluster node iterator functions */
LIBNEXCACHE_API void nexcacheClusterInitNodeIterator(nexcacheClusterNodeIterator *iter,
                                                 nexcacheClusterContext *cc);
LIBNEXCACHE_API nexcacheClusterNode *nexcacheClusterNodeNext(nexcacheClusterNodeIterator *iter);

/* Helper functions */
LIBNEXCACHE_API unsigned int nexcacheClusterGetSlotByKey(char *key);
LIBNEXCACHE_API nexcacheClusterNode *nexcacheClusterGetNodeByKey(nexcacheClusterContext *cc,
                                                           char *key);

#ifdef __cplusplus
}
#endif

#endif /* NEXCACHE_CLUSTER_H */
