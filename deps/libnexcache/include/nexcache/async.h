/*
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2011, Pieter Noordhuis <pcnoordhuis at gmail dot com>
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

#ifndef NEXCACHE_ASYNC_H
#define NEXCACHE_ASYNC_H
#include "nexcache.h"
#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For the async cluster attach functions. */
#if defined(__GNUC__) || defined(__clang__)
#define NEXCACHE_UNUSED __attribute__((unused))
#else
#define NEXCACHE_UNUSED
#endif

struct nexcacheAsyncContext; /* need forward declaration of nexcacheAsyncContext */
struct dict;               /* dictionary header is included in async.c */

/* Reply callback prototype and container */
typedef void(nexcacheCallbackFn)(struct nexcacheAsyncContext *, void *, void *);
typedef struct nexcacheCallback {
    struct nexcacheCallback *next; /* simple singly linked list */
    nexcacheCallbackFn *fn;
    int pending_subs;
    int unsubscribe_sent;
    void *privdata;
    int subscribed;
} nexcacheCallback;

/* List of callbacks for either regular replies or pub/sub */
typedef struct nexcacheCallbackList {
    nexcacheCallback *head, *tail;
} nexcacheCallbackList;

/* Connection callback prototypes */
typedef void(nexcacheDisconnectCallback)(const struct nexcacheAsyncContext *, int status);
typedef void(nexcacheConnectCallback)(struct nexcacheAsyncContext *, int status);
typedef void(nexcacheTimerCallback)(void *timer, void *privdata);

/* Context for an async connection to NexCache */
typedef struct nexcacheAsyncContext {
    /* Hold the regular context, so it can be realloc'ed. */
    nexcacheContext c;

    /* Setup error flags so they can be used directly. */
    int err;
    char *errstr;

    /* Not used by libnexcache */
    void *data;
    void (*dataCleanup)(void *privdata);

    /* Event library data and hooks */
    struct {
        void *data;

        /* Hooks that are called when the library expects to start
         * reading/writing. These functions should be idempotent. */
        void (*addRead)(void *privdata);
        void (*delRead)(void *privdata);
        void (*addWrite)(void *privdata);
        void (*delWrite)(void *privdata);
        void (*cleanup)(void *privdata);
        void (*scheduleTimer)(void *privdata, struct timeval tv);
    } ev;

    /* Called when either the connection is terminated due to an error or per
     * user request. The status is set accordingly (NEXCACHE_OK, NEXCACHE_ERR). */
    nexcacheDisconnectCallback *onDisconnect;

    /* Called when the first write event was received. */
    nexcacheConnectCallback *onConnect;

    /* Regular command callbacks */
    nexcacheCallbackList replies;

    /* Address used for connect() */
    struct sockaddr *saddr;
    size_t addrlen;

    /* Subscription callbacks */
    struct {
        nexcacheCallbackList replies;
        struct dict *channels;
        struct dict *patterns;
        struct dict *schannels;
        int pending_unsubs;
    } sub;

    /* Any configured RESP3 PUSH handler */
    nexcacheAsyncPushFn *push_cb;
} nexcacheAsyncContext;

LIBNEXCACHE_API nexcacheAsyncContext *nexcacheAsyncConnectWithOptions(const nexcacheOptions *options);
LIBNEXCACHE_API nexcacheAsyncContext *nexcacheAsyncConnect(const char *ip, int port);
LIBNEXCACHE_API nexcacheAsyncContext *nexcacheAsyncConnectBind(const char *ip, int port, const char *source_addr);
LIBNEXCACHE_API nexcacheAsyncContext *nexcacheAsyncConnectBindWithReuse(const char *ip, int port,
                                                                  const char *source_addr);
LIBNEXCACHE_API nexcacheAsyncContext *nexcacheAsyncConnectUnix(const char *path);
LIBNEXCACHE_API int nexcacheAsyncSetConnectCallback(nexcacheAsyncContext *ac, nexcacheConnectCallback *fn);
LIBNEXCACHE_API int nexcacheAsyncSetDisconnectCallback(nexcacheAsyncContext *ac, nexcacheDisconnectCallback *fn);

LIBNEXCACHE_API nexcacheAsyncPushFn *nexcacheAsyncSetPushCallback(nexcacheAsyncContext *ac, nexcacheAsyncPushFn *fn);
LIBNEXCACHE_API int nexcacheAsyncSetTimeout(nexcacheAsyncContext *ac, struct timeval tv);
LIBNEXCACHE_API void nexcacheAsyncDisconnect(nexcacheAsyncContext *ac);
LIBNEXCACHE_API void nexcacheAsyncFree(nexcacheAsyncContext *ac);

/* Handle read/write events */
LIBNEXCACHE_API void nexcacheAsyncHandleRead(nexcacheAsyncContext *ac);
LIBNEXCACHE_API void nexcacheAsyncHandleWrite(nexcacheAsyncContext *ac);
LIBNEXCACHE_API void nexcacheAsyncHandleTimeout(nexcacheAsyncContext *ac);
LIBNEXCACHE_API void nexcacheAsyncRead(nexcacheAsyncContext *ac);
LIBNEXCACHE_API void nexcacheAsyncWrite(nexcacheAsyncContext *ac);

/* Command functions for an async context. Write the command to the
 * output buffer and register the provided callback. */
LIBNEXCACHE_API int nexcachevAsyncCommand(nexcacheAsyncContext *ac, nexcacheCallbackFn *fn, void *privdata, const char *format, va_list ap);
LIBNEXCACHE_API int nexcacheAsyncCommand(nexcacheAsyncContext *ac, nexcacheCallbackFn *fn, void *privdata, const char *format, ...);
LIBNEXCACHE_API int nexcacheAsyncCommandArgv(nexcacheAsyncContext *ac, nexcacheCallbackFn *fn, void *privdata, int argc, const char **argv, const size_t *argvlen);
LIBNEXCACHE_API int nexcacheAsyncFormattedCommand(nexcacheAsyncContext *ac, nexcacheCallbackFn *fn, void *privdata, const char *cmd, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NEXCACHE_ASYNC_H */
