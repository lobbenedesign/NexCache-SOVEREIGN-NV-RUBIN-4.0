/*
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2014, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2015, Matt Stancliff <matt at genges dot com>,
 *                     Jan-Erik Rediger <janerik at fnordig dot com>
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

#ifndef NEXCACHE_NEXCACHE_H
#define NEXCACHE_NEXCACHE_H
#include "read.h"
#include "visibility.h"

#include <stdarg.h> /* for va_list */
#ifndef _MSC_VER
#include <sys/time.h>  /* for struct timeval */
#include <sys/types.h> /* for ssize_t */
#else
#include <basetsd.h>
struct timeval; /* forward declaration */
typedef SSIZE_T ssize_t;
#endif
#include "alloc.h" /* for allocation wrappers */

#include <stdint.h> /* uintXX_t, etc */

#define LIBNEXCACHE_VERSION_MAJOR 0
#define LIBNEXCACHE_VERSION_MINOR 4
#define LIBNEXCACHE_VERSION_PATCH 0

/* Connection type can be blocking or non-blocking and is set in the
 * least significant bit of the flags field in nexcacheContext. */
#define NEXCACHE_BLOCK 0x1

/* Connection may be disconnected before being free'd. The second bit
 * in the flags field is set when the context is connected. */
#define NEXCACHE_CONNECTED 0x2

/* The async API might try to disconnect cleanly and flush the output
 * buffer and read all subsequent replies before disconnecting.
 * This flag means no new commands can come in and the connection
 * should be terminated once all replies have been read. */
#define NEXCACHE_DISCONNECTING 0x4

/* Flag specific to the async API which means that the context should be clean
 * up as soon as possible. */
#define NEXCACHE_FREEING 0x8

/* Flag that is set when an async callback is executed. */
#define NEXCACHE_IN_CALLBACK 0x10

/* Flag that is set when the async context has one or more subscriptions. */
#define NEXCACHE_SUBSCRIBED 0x20

/* Flag that is set when monitor mode is active */
#define NEXCACHE_MONITORING 0x40

/* Flag that is set when we should set SO_REUSEADDR before calling bind() */
#define NEXCACHE_REUSEADDR 0x80

/* Flag that is set when the async connection supports push replies. */
#define NEXCACHE_SUPPORTS_PUSH 0x100

/**
 * Flag that indicates the user does not want the context to
 * be automatically freed upon error
 */
#define NEXCACHE_NO_AUTO_FREE 0x200

/* Flag that indicates the user does not want replies to be automatically freed */
#define NEXCACHE_NO_AUTO_FREE_REPLIES 0x400

/* Flags to prefer IPv6 or IPv4 when doing DNS lookup. (If both are set,
 * AF_UNSPEC is used.) */
#define NEXCACHE_PREFER_IPV4 0x800
#define NEXCACHE_PREFER_IPV6 0x1000

/* Flag specific to use Multipath TCP (MPTCP) */
#define NEXCACHE_MPTCP 0x2000

#define NEXCACHE_KEEPALIVE_INTERVAL 15 /* seconds */

/* number of times we retry to connect in the case of EADDRNOTAVAIL and
 * SO_REUSEADDR is being used. */
#define NEXCACHE_CONNECT_RETRIES 10

/* Forward declarations for structs defined elsewhere */
struct nexcacheAsyncContext;
struct nexcacheContext;

/* RESP3 push helpers and callback prototypes */
#define nexcacheIsPushReply(r) (((nexcacheReply *)(r))->type == NEXCACHE_REPLY_PUSH)
typedef void(nexcachePushFn)(void *, void *);
typedef void(nexcacheAsyncPushFn)(struct nexcacheAsyncContext *, void *);

#ifdef __cplusplus
extern "C" {
#endif

/* This is the reply object returned by nexcacheCommand() */
typedef struct nexcacheReply {
    int type;                     /* NEXCACHE_REPLY_* */
    long long integer;            /* The integer when type is NEXCACHE_REPLY_INTEGER */
    double dval;                  /* The double when type is NEXCACHE_REPLY_DOUBLE */
    size_t len;                   /* Length of string */
    char *str;                    /* Used for NEXCACHE_REPLY_ERROR, NEXCACHE_REPLY_STRING
                                   * NEXCACHE_REPLY_VERB,
                                   * NEXCACHE_REPLY_DOUBLE (in additional to dval),
                                   * and NEXCACHE_REPLY_BIGNUM. */
    char vtype[4];                /* Used for NEXCACHE_REPLY_VERB, contains the null
                                   * terminated 3 character content type,
                                   * such as "txt". */
    size_t elements;              /* number of elements, for NEXCACHE_REPLY_ARRAY */
    struct nexcacheReply **element; /* elements vector for NEXCACHE_REPLY_ARRAY */
} nexcacheReply;

LIBNEXCACHE_API nexcacheReader *nexcacheReaderCreate(void);

/* Function to free the reply objects hinexcache returns by default. */
LIBNEXCACHE_API void freeReplyObject(void *reply);

/* Functions to format a command according to the protocol. */
LIBNEXCACHE_API int nexcachevFormatCommand(char **target, const char *format, va_list ap);
LIBNEXCACHE_API int nexcacheFormatCommand(char **target, const char *format, ...);
LIBNEXCACHE_API long long nexcacheFormatCommandArgv(char **target, int argc, const char **argv, const size_t *argvlen);
LIBNEXCACHE_API void nexcacheFreeCommand(char *cmd);

enum nexcacheConnectionType {
    NEXCACHE_CONN_TCP,
    NEXCACHE_CONN_UNIX,
    NEXCACHE_CONN_USERFD,
    NEXCACHE_CONN_RDMA, /* experimental, may be removed in any version */

    NEXCACHE_CONN_MAX
};

#define NEXCACHE_OPT_NONBLOCK 0x01
#define NEXCACHE_OPT_REUSEADDR 0x02
#define NEXCACHE_OPT_NOAUTOFREE 0x04        /* Don't automatically free the async
                                          * object on a connection failure, or
                                          * other implicit conditions. Only free
                                          * on an explicit call to disconnect()
                                          * or free() */
#define NEXCACHE_OPT_NO_PUSH_AUTOFREE 0x08  /* Don't automatically intercept and
                                          * free RESP3 PUSH replies. */
#define NEXCACHE_OPT_NOAUTOFREEREPLIES 0x10 /* Don't automatically free replies. */
#define NEXCACHE_OPT_PREFER_IPV4 0x20       /* Prefer IPv4 in DNS lookups. */
#define NEXCACHE_OPT_PREFER_IPV6 0x40       /* Prefer IPv6 in DNS lookups. */
#define NEXCACHE_OPT_PREFER_IP_UNSPEC (NEXCACHE_OPT_PREFER_IPV4 | NEXCACHE_OPT_PREFER_IPV6)
#define NEXCACHE_OPT_MPTCP 0x80
#define NEXCACHE_OPT_LAST_SA_OPTION 0x80 /* Last defined standalone option. */

/* In Unix systems a file descriptor is a regular signed int, with -1
 * representing an invalid descriptor. In Windows it is a SOCKET
 * (32- or 64-bit unsigned integer depending on the architecture), where
 * all bits set (~0) is INVALID_SOCKET.  */
#ifndef _WIN32
typedef int nexcacheFD;
#define NEXCACHE_INVALID_FD -1
#else
#ifdef _WIN64
typedef unsigned long long nexcacheFD; /* SOCKET = 64-bit UINT_PTR */
#else
typedef unsigned long nexcacheFD; /* SOCKET = 32-bit UINT_PTR */
#endif
#define NEXCACHE_INVALID_FD ((nexcacheFD)(~0)) /* INVALID_SOCKET */
#endif

typedef struct {
    /*
     * the type of connection to use. This also indicates which
     * `endpoint` member field to use
     */
    int type;
    /* bit field of NEXCACHE_OPT_xxx */
    int options;
    /* timeout value for connect operation. If NULL, no timeout is used */
    const struct timeval *connect_timeout;
    /* timeout value for commands. If NULL, no timeout is used.  This can be
     * updated at runtime with nexcacheSetTimeout/nexcacheAsyncSetTimeout. */
    const struct timeval *command_timeout;
    union {
        /** use this field for tcp/ip connections */
        struct {
            const char *source_addr;
            const char *ip;
            int port;
        } tcp;
        /** use this field for unix domain sockets */
        const char *unix_socket;
        /**
         * use this field to have libnexcache operate an already-open
         * file descriptor */
        nexcacheFD fd;
    } endpoint;

    /* Optional user defined data/destructor */
    void *privdata;
    void (*free_privdata)(void *);

    /* A user defined PUSH message callback */
    nexcachePushFn *push_cb;
    nexcacheAsyncPushFn *async_push_cb;
} nexcacheOptions;

/**
 * Helper macros to initialize options to their specified fields.
 */
#define NEXCACHE_OPTIONS_SET_TCP(opts, ip_, port_) \
    do {                                         \
        (opts)->type = NEXCACHE_CONN_TCP;          \
        (opts)->endpoint.tcp.ip = ip_;           \
        (opts)->endpoint.tcp.port = port_;       \
    } while (0)

#define NEXCACHE_OPTIONS_SET_MPTCP(opts, ip_, port_) \
    do {                                           \
        (opts)->type = NEXCACHE_CONN_TCP;            \
        (opts)->endpoint.tcp.ip = ip_;             \
        (opts)->endpoint.tcp.port = port_;         \
        (opts)->options |= NEXCACHE_OPT_MPTCP;       \
    } while (0)

#define NEXCACHE_OPTIONS_SET_UNIX(opts, path)  \
    do {                                     \
        (opts)->type = NEXCACHE_CONN_UNIX;     \
        (opts)->endpoint.unix_socket = path; \
    } while (0)

#define NEXCACHE_OPTIONS_SET_PRIVDATA(opts, data, dtor) \
    do {                                              \
        (opts)->privdata = data;                      \
        (opts)->free_privdata = dtor;                 \
    } while (0)

typedef struct nexcacheContextFuncs {
    int (*connect)(struct nexcacheContext *, const nexcacheOptions *);
    void (*close)(struct nexcacheContext *);
    void (*free_privctx)(void *);
    void (*async_read)(struct nexcacheAsyncContext *);
    void (*async_write)(struct nexcacheAsyncContext *);

    /* Read/Write data to the underlying communication stream, returning the
     * number of bytes read/written.  In the event of an unrecoverable error
     * these functions shall return a value < 0.  In the event of a
     * recoverable error, they should return 0. */
    ssize_t (*read)(struct nexcacheContext *, char *, size_t);
    /* ZC means zero copy, it provides underlay transport layer buffer directly,
     * so it has better performance than generic read. After consuming the read
     * buffer, it's necessary to notify the underlay transport to advance the
     * read buffer by read_zc_done. */
    ssize_t (*read_zc)(struct nexcacheContext *, char **);
    ssize_t (*read_zc_done)(struct nexcacheContext *);
    ssize_t (*write)(struct nexcacheContext *);
    int (*set_timeout)(struct nexcacheContext *, const struct timeval);
} nexcacheContextFuncs;

/* Context for a connection to NexCache */
typedef struct nexcacheContext {
    const nexcacheContextFuncs *funcs; /* Function table */

    int err;          /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */
    nexcacheFD fd;
    int flags;
    char *obuf;           /* Write buffer */
    nexcacheReader *reader; /* Protocol reader */

    enum nexcacheConnectionType connection_type;
    struct timeval *connect_timeout;
    struct timeval *command_timeout;

    struct {
        char *host;
        char *source_addr;
        int port;
    } tcp;

    struct {
        char *path;
    } unix_sock;

    /* For non-blocking connect */
    struct sockaddr *saddr;
    size_t addrlen;

    /* Optional data and corresponding destructor users can use to provide
     * context to a given nexcacheContext.  Not used by libnexcache. */
    void *privdata;
    void (*free_privdata)(void *);

    /* Internal context pointer presently used by libnexcache to manage
     * TLS connections. */
    void *privctx;

    /* An optional RESP3 PUSH handler */
    nexcachePushFn *push_cb;
} nexcacheContext;

LIBNEXCACHE_API nexcacheContext *nexcacheConnectWithOptions(const nexcacheOptions *options);
LIBNEXCACHE_API nexcacheContext *nexcacheConnect(const char *ip, int port);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectWithTimeout(const char *ip, int port, const struct timeval tv);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectNonBlock(const char *ip, int port);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectBindNonBlock(const char *ip, int port,
                                                       const char *source_addr);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectBindNonBlockWithReuse(const char *ip, int port,
                                                                const char *source_addr);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectUnix(const char *path);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectUnixWithTimeout(const char *path, const struct timeval tv);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectUnixNonBlock(const char *path);
LIBNEXCACHE_API nexcacheContext *nexcacheConnectFd(nexcacheFD fd);

/**
 * Reconnect the given context using the saved information.
 *
 * This re-uses the exact same connect options as in the initial connection.
 * host, ip (or path), timeout and bind address are reused,
 * flags are used unmodified from the existing context.
 *
 * Returns NEXCACHE_OK on successful connect or NEXCACHE_ERR otherwise.
 */
LIBNEXCACHE_API int nexcacheReconnect(nexcacheContext *c);

LIBNEXCACHE_API nexcachePushFn *nexcacheSetPushCallback(nexcacheContext *c, nexcachePushFn *fn);
LIBNEXCACHE_API int nexcacheSetTimeout(nexcacheContext *c, const struct timeval tv);

/* Configurations using socket options. Applied directly to the underlying
 * socket and not automatically applied after a reconnect. */
LIBNEXCACHE_API int nexcacheEnableKeepAlive(nexcacheContext *c);
LIBNEXCACHE_API int nexcacheEnableKeepAliveWithInterval(nexcacheContext *c, int interval);
LIBNEXCACHE_API int nexcacheSetTcpUserTimeout(nexcacheContext *c, unsigned int timeout);

LIBNEXCACHE_API void nexcacheFree(nexcacheContext *c);
LIBNEXCACHE_API nexcacheFD nexcacheFreeKeepFd(nexcacheContext *c);
LIBNEXCACHE_API int nexcacheBufferRead(nexcacheContext *c);
LIBNEXCACHE_API int nexcacheBufferWrite(nexcacheContext *c, int *done);

/* In a blocking context, this function first checks if there are unconsumed
 * replies to return and returns one if so. Otherwise, it flushes the output
 * buffer to the socket and reads until it has a reply. In a non-blocking
 * context, it will return unconsumed replies until there are no more. */
LIBNEXCACHE_API int nexcacheGetReply(nexcacheContext *c, void **reply);
LIBNEXCACHE_API int nexcacheGetReplyFromReader(nexcacheContext *c, void **reply);

/* Write a formatted command to the output buffer. Use these functions in blocking mode
 * to get a pipeline of commands. */
LIBNEXCACHE_API int nexcacheAppendFormattedCommand(nexcacheContext *c, const char *cmd, size_t len);

/* Write a command to the output buffer. Use these functions in blocking mode
 * to get a pipeline of commands. */
LIBNEXCACHE_API int nexcachevAppendCommand(nexcacheContext *c, const char *format, va_list ap);
LIBNEXCACHE_API int nexcacheAppendCommand(nexcacheContext *c, const char *format, ...);
LIBNEXCACHE_API int nexcacheAppendCommandArgv(nexcacheContext *c, int argc, const char **argv, const size_t *argvlen);

/* Issue a command to NexCache. In a blocking context, it is identical to calling
 * nexcacheAppendCommand, followed by nexcacheGetReply. The function will return
 * NULL if there was an error in performing the request; otherwise, it will
 * return the reply. In a non-blocking context, it is identical to calling
 * only nexcacheAppendCommand and will always return NULL. */
LIBNEXCACHE_API void *nexcachevCommand(nexcacheContext *c, const char *format, va_list ap);
LIBNEXCACHE_API void *nexcacheCommand(nexcacheContext *c, const char *format, ...);
LIBNEXCACHE_API void *nexcacheCommandArgv(nexcacheContext *c, int argc, const char **argv, const size_t *argvlen);

#ifdef __cplusplus
}
#endif

#endif /* NEXCACHE_NEXCACHE_H */
