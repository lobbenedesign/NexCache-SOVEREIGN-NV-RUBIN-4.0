/* Extracted from anet.c to work properly with Hinexcache error reporting.
 *
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

#include "fmacros.h"
#include "win32.h"

#include "net.h"

#include "async.h"
#include "sockcompat.h"
#include "nexcache_private.h"

#include <sds.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

void nexcacheNetClose(nexcacheContext *c) {
    if (c && c->fd != NEXCACHE_INVALID_FD) {
        close(c->fd);
        c->fd = NEXCACHE_INVALID_FD;
    }
}

static ssize_t nexcacheNetRead(nexcacheContext *c, char *buf, size_t bufcap) {
    ssize_t nread = recv(c->fd, buf, bufcap, 0);
    if (nread == -1) {
        if ((errno == EWOULDBLOCK && !(c->flags & NEXCACHE_BLOCK)) || (errno == EINTR)) {
            /* Try again later */
            return 0;
        } else if (errno == ETIMEDOUT && (c->flags & NEXCACHE_BLOCK)) {
            /* especially in windows */
            nexcacheSetError(c, NEXCACHE_ERR_TIMEOUT, "recv timeout");
            return -1;
        } else {
            nexcacheSetError(c, NEXCACHE_ERR_IO, strerror(errno));
            return -1;
        }
    } else if (nread == 0) {
        nexcacheSetError(c, NEXCACHE_ERR_EOF, "Server closed the connection");
        return -1;
    } else {
        return nread;
    }
}

static ssize_t nexcacheNetWrite(nexcacheContext *c) {
    ssize_t nwritten;

    nwritten = send(c->fd, c->obuf, sdslen(c->obuf), 0);
    if (nwritten < 0) {
        if ((errno == EWOULDBLOCK && !(c->flags & NEXCACHE_BLOCK)) || (errno == EINTR)) {
            /* Try again */
            return 0;
        } else {
            nexcacheSetError(c, NEXCACHE_ERR_IO, strerror(errno));
            return -1;
        }
    }

    return nwritten;
}

static void nexcacheSetErrorFromErrno(nexcacheContext *c, int type, const char *prefix) {
    int errorno = errno; /* snprintf() may change errno */
    char buf[128] = {0};
    size_t len = 0;

    if (prefix != NULL)
        len = snprintf(buf, sizeof(buf), "%s: ", prefix);
    strerror_r(errorno, (char *)(buf + len), sizeof(buf) - len);
    nexcacheSetError(c, type, buf);
}

static int nexcacheSetReuseAddr(nexcacheContext *c) {
    int on = 1;
    if (setsockopt(c->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, NULL);
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

static int nexcacheCreateSocket(nexcacheContext *c, int type) {
    nexcacheFD s;
    if ((s = socket(type, SOCK_STREAM, 0)) == NEXCACHE_INVALID_FD) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, NULL);
        return NEXCACHE_ERR;
    }
    c->fd = s;
    if (type == AF_INET) {
        if (nexcacheSetReuseAddr(c) == NEXCACHE_ERR) {
            return NEXCACHE_ERR;
        }
    }
    return NEXCACHE_OK;
}

static int nexcacheSetBlocking(nexcacheContext *c, int blocking) {
#ifndef _WIN32
    int flags;

    /* Set the socket nonblocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(c->fd, F_GETFL)) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "fcntl(F_GETFL)");
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if (fcntl(c->fd, F_SETFL, flags) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "fcntl(F_SETFL)");
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }
#else
    u_long mode = blocking ? 0 : 1;
    if (ioctl(c->fd, FIONBIO, &mode) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "ioctl(FIONBIO)");
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }
#endif /* _WIN32 */
    return NEXCACHE_OK;
}

int nexcacheKeepAlive(nexcacheContext *c, int interval) {
    int val = 1;
    nexcacheFD fd = c->fd;

    /* TCP_KEEPALIVE makes no sense with AF_UNIX connections */
    if (c->connection_type == NEXCACHE_CONN_UNIX)
        return NEXCACHE_ERR;

#ifndef _WIN32
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(errno));
        return NEXCACHE_ERR;
    }

    val = interval;

#if defined(__APPLE__) && defined(__MACH__)
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &val, sizeof(val)) < 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(errno));
        return NEXCACHE_ERR;
    }
#else
#if defined(__GLIBC__) && !defined(__FreeBSD_kernel__)
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(errno));
        return NEXCACHE_ERR;
    }

    val = interval / 3;
    if (val == 0)
        val = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(errno));
        return NEXCACHE_ERR;
    }

    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(errno));
        return NEXCACHE_ERR;
    }
#endif
#endif
#else
    int res;

    res = win32_nexcacheKeepAlive(fd, interval * 1000);
    if (res != 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, strerror(res));
        return NEXCACHE_ERR;
    }
#endif
    return NEXCACHE_OK;
}

int nexcacheSetTcpNoDelay(nexcacheContext *c) {
    int yes = 1;
    if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "setsockopt(TCP_NODELAY)");
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

int nexcacheContextSetTcpUserTimeout(nexcacheContext *c, unsigned int timeout) {
    int res;
#ifdef TCP_USER_TIMEOUT
    res = setsockopt(c->fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
#else
    res = -1;
    errno = ENOTSUP;
    (void)timeout;
#endif
    if (res == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "setsockopt(TCP_USER_TIMEOUT)");
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

static long nexcachePollMillis(void) {
#ifndef _MSC_VER
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000) + now.tv_nsec / 1000000;
#else
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10;
#endif
}

static int nexcacheContextWaitReady(nexcacheContext *c, long msec) {
    struct pollfd wfd;
    long end;
    int res;

    if (errno != EINPROGRESS) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, NULL);
        nexcacheNetClose(c);
        return NEXCACHE_ERR;
    }

    wfd.fd = c->fd;
    wfd.events = POLLOUT;
    end = msec >= 0 ? nexcachePollMillis() + msec : 0;

    while ((res = poll(&wfd, 1, msec)) <= 0) {
        if (res < 0 && errno != EINTR) {
            nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "poll(2)");
            nexcacheNetClose(c);
            return NEXCACHE_ERR;
        } else if (res == 0 || (msec >= 0 && nexcachePollMillis() >= end)) {
            errno = ETIMEDOUT;
            nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, NULL);
            nexcacheNetClose(c);
            return NEXCACHE_ERR;
        } else {
            /* res < 0 && errno == EINTR, try again */
        }
    }

    if (nexcacheCheckConnectDone(c, &res) != NEXCACHE_OK || res == 0) {
        nexcacheCheckSocketError(c);
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

int nexcacheCheckConnectDone(nexcacheContext *c, int *completed) {
    int rc = connect(c->fd, (const struct sockaddr *)c->saddr, c->addrlen);
    if (rc == 0) {
        *completed = 1;
        return NEXCACHE_OK;
    }
    int error = errno;
    if (error == EINPROGRESS) {
        /* must check error to see if connect failed.  Get the socket error */
        int fail, so_error;
        socklen_t optlen = sizeof(so_error);
        fail = getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen);
        if (fail == 0) {
            if (so_error == 0) {
                /* Socket is connected! */
                *completed = 1;
                return NEXCACHE_OK;
            }
            /* connection error; */
            errno = so_error;
            error = so_error;
        }
    }
    switch (error) {
    case EISCONN:
        *completed = 1;
        return NEXCACHE_OK;
    case EALREADY:
    case EWOULDBLOCK:
        *completed = 0;
        return NEXCACHE_OK;
    default:
        return NEXCACHE_ERR;
    }
}

int nexcacheCheckSocketError(nexcacheContext *c) {
    int err = 0, errno_saved = errno;
    socklen_t errlen = sizeof(err);

    if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "getsockopt(SO_ERROR)");
        return NEXCACHE_ERR;
    }

    if (err == 0) {
        err = errno_saved;
    }

    if (err) {
        errno = err;
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, NULL);
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

int nexcacheTcpSetTimeout(nexcacheContext *c, const struct timeval tv) {
    const void *to_ptr = &tv;
    size_t to_sz = sizeof(tv);

    if (setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, to_ptr, to_sz) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "setsockopt(SO_RCVTIMEO)");
        return NEXCACHE_ERR;
    }
    if (setsockopt(c->fd, SOL_SOCKET, SO_SNDTIMEO, to_ptr, to_sz) == -1) {
        nexcacheSetErrorFromErrno(c, NEXCACHE_ERR_IO, "setsockopt(SO_SNDTIMEO)");
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

#ifdef IPPROTO_MPTCP
int nexcacheHasMptcp(void) {
    return 1;
}

/* XXX: Until glibc 2.41, getaddrinfo with hints.ai_protocol of IPPROTO_MPTCP leads error.
 * Use hints.ai_protocol IPPROTO_IP (0) or IPPROTO_TCP (6) to resolve address and overwrite
 * it when MPTCP is enabled.
 * Ref: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/testing/selftests/net/mptcp/mptcp_connect.c
 *      https://sourceware.org/git/?p=glibc.git;a=commit;h=a8e9022e0f829d44a818c642fc85b3bfbd26a514
 */
static int nexcacheTcpGetProtocol(int is_mptcp_enabled) {
    return is_mptcp_enabled ? IPPROTO_MPTCP : IPPROTO_TCP;
}

#else
int nexcacheHasMptcp(void) {
    return 0;
}

static int nexcacheTcpGetProtocol(int is_mptcp_enabled) {
    assert(!is_mptcp_enabled);
    (void)is_mptcp_enabled; /* Suppress unused warning when NDEBUG is defined. */
    return IPPROTO_TCP;
}
#endif /* IPPROTO_MPTCP */

int nexcacheContextConnectTcp(nexcacheContext *c, const nexcacheOptions *options) {
    const struct timeval *timeout = options->connect_timeout;
    const char *addr = options->endpoint.tcp.ip;
    const char *source_addr = options->endpoint.tcp.source_addr;
    int port = options->endpoint.tcp.port;
    nexcacheFD s;
    int rv, n;
    char _port[6]; /* strlen("65535"); */
    struct addrinfo hints, *servinfo, *bservinfo, *p, *b;
    int blocking = (c->flags & NEXCACHE_BLOCK);
    int reuseaddr = (c->flags & NEXCACHE_REUSEADDR);
    int reuses = 0;
    long timeout_msec = -1;

    servinfo = NULL;
    c->connection_type = NEXCACHE_CONN_TCP;
    c->tcp.port = port;

    /* We need to take possession of the passed parameters
     * to make them reusable for a reconnect.
     * We also carefully check we don't free data we already own,
     * as in the case of the reconnect method.
     *
     * This is a bit ugly, but atleast it works and doesn't leak memory.
     **/
    if (c->tcp.host != addr) {
        vk_free(c->tcp.host);

        c->tcp.host = vk_strdup(addr);
        if (c->tcp.host == NULL)
            goto oom;
    }

    if (timeout) {
        if (nexcacheContextUpdateConnectTimeout(c, timeout) == NEXCACHE_ERR)
            goto oom;
    } else {
        vk_free(c->connect_timeout);
        c->connect_timeout = NULL;
    }

    if (nexcacheConnectTimeoutMsec(c, &timeout_msec) != NEXCACHE_OK) {
        goto error;
    }

    if (source_addr == NULL) {
        vk_free(c->tcp.source_addr);
        c->tcp.source_addr = NULL;
    } else if (c->tcp.source_addr != source_addr) {
        vk_free(c->tcp.source_addr);
        c->tcp.source_addr = vk_strdup(source_addr);
    }

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* DNS lookup. To use dual stack, set both flags to prefer both IPv4 and
     * IPv6. By default, for historical reasons, we try IPv4 first and then we
     * try IPv6 only if no IPv4 address was found. */
    if (c->flags & NEXCACHE_PREFER_IPV6 && c->flags & NEXCACHE_PREFER_IPV4)
        hints.ai_family = AF_UNSPEC;
    else if (c->flags & NEXCACHE_PREFER_IPV6)
        hints.ai_family = AF_INET6;
    else
        hints.ai_family = AF_INET;

    rv = getaddrinfo(c->tcp.host, _port, &hints, &servinfo);
    if (rv != 0 && hints.ai_family != AF_UNSPEC) {
        /* Try again with the other IP version. */
        hints.ai_family = (hints.ai_family == AF_INET) ? AF_INET6 : AF_INET;
        rv = getaddrinfo(c->tcp.host, _port, &hints, &servinfo);
    }
    if (rv != 0) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, gai_strerror(rv));
        return NEXCACHE_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
    addrretry:
        if ((s = socket(p->ai_family, p->ai_socktype, nexcacheTcpGetProtocol(c->flags & NEXCACHE_MPTCP))) == NEXCACHE_INVALID_FD)
            continue;

        c->fd = s;
        if (nexcacheSetBlocking(c, 0) != NEXCACHE_OK)
            goto error;
        if (c->tcp.source_addr) {
            int bound = 0;
            /* Using getaddrinfo saves us from self-determining IPv4 vs IPv6 */
            if ((rv = getaddrinfo(c->tcp.source_addr, NULL, &hints, &bservinfo)) != 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "Can't get addr: %s", gai_strerror(rv));
                nexcacheSetError(c, NEXCACHE_ERR_OTHER, buf);
                goto error;
            }

            if (reuseaddr) {
                n = 1;
                if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&n,
                               sizeof(n)) < 0) {
                    freeaddrinfo(bservinfo);
                    goto error;
                }
            }

            for (b = bservinfo; b != NULL; b = b->ai_next) {
                if (bind(s, b->ai_addr, b->ai_addrlen) != -1) {
                    bound = 1;
                    break;
                }
            }
            freeaddrinfo(bservinfo);
            if (!bound) {
                char buf[128];
                snprintf(buf, sizeof(buf), "Can't bind socket: %s", strerror(errno));
                nexcacheSetError(c, NEXCACHE_ERR_OTHER, buf);
                goto error;
            }
        }

        /* For repeat connection */
        vk_free(c->saddr);
        c->saddr = vk_malloc(p->ai_addrlen);
        if (c->saddr == NULL)
            goto oom;

        memcpy(c->saddr, p->ai_addr, p->ai_addrlen);
        c->addrlen = p->ai_addrlen;

        if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
            if (errno == EHOSTUNREACH) {
                nexcacheNetClose(c);
                continue;
            } else if (errno == EINPROGRESS) {
                if (blocking) {
                    goto wait_for_ready;
                }
                /* This is ok.
                 * Note that even when it's in blocking mode, we unset blocking
                 * for `connect()`
                 */
            } else if (errno == EADDRNOTAVAIL && reuseaddr) {
                if (++reuses >= NEXCACHE_CONNECT_RETRIES) {
                    goto error;
                } else {
                    nexcacheNetClose(c);
                    goto addrretry;
                }
            } else {
            wait_for_ready:
                if (nexcacheContextWaitReady(c, timeout_msec) != NEXCACHE_OK)
                    goto error;
                if (nexcacheSetTcpNoDelay(c) != NEXCACHE_OK)
                    goto error;
            }
        }
        if (blocking && nexcacheSetBlocking(c, 1) != NEXCACHE_OK)
            goto error;

        c->flags |= NEXCACHE_CONNECTED;
        rv = NEXCACHE_OK;
        goto end;
    }
    if (p == NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Can't create socket: %s", strerror(errno));
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, buf);
        goto error;
    }

oom:
    nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
error:
    rv = NEXCACHE_ERR;
end:
    if (servinfo) {
        freeaddrinfo(servinfo);
    }

    return rv; // Need to return NEXCACHE_OK if alright
}

static int nexcacheContextConnectUnix(nexcacheContext *c, const nexcacheOptions *options) {
#ifndef _WIN32
    const struct timeval *timeout = options->connect_timeout;
    const char *path = options->endpoint.unix_socket;
    int blocking = (c->flags & NEXCACHE_BLOCK);
    struct sockaddr_un *sa;
    long timeout_msec = -1;

    if (nexcacheCreateSocket(c, AF_UNIX) < 0)
        return NEXCACHE_ERR;
    if (nexcacheSetBlocking(c, 0) != NEXCACHE_OK)
        return NEXCACHE_ERR;

    c->connection_type = NEXCACHE_CONN_UNIX;
    if (c->unix_sock.path != path) {
        vk_free(c->unix_sock.path);

        c->unix_sock.path = vk_strdup(path);
        if (c->unix_sock.path == NULL)
            goto oom;
    }

    if (timeout) {
        if (nexcacheContextUpdateConnectTimeout(c, timeout) == NEXCACHE_ERR)
            goto oom;
    } else {
        vk_free(c->connect_timeout);
        c->connect_timeout = NULL;
    }

    if (nexcacheConnectTimeoutMsec(c, &timeout_msec) != NEXCACHE_OK)
        return NEXCACHE_ERR;

    /* Don't leak sockaddr if we're reconnecting */
    if (c->saddr)
        vk_free(c->saddr);

    sa = (struct sockaddr_un *)(c->saddr = vk_malloc(sizeof(struct sockaddr_un)));
    if (sa == NULL)
        goto oom;

    c->addrlen = sizeof(struct sockaddr_un);
    sa->sun_family = AF_UNIX;
    strncpy(sa->sun_path, path, sizeof(sa->sun_path) - 1);
    if (connect(c->fd, (struct sockaddr *)sa, sizeof(*sa)) == -1) {
        if (errno == EINPROGRESS && !blocking) {
            /* This is ok. */
        } else {
            if (nexcacheContextWaitReady(c, timeout_msec) != NEXCACHE_OK)
                return NEXCACHE_ERR;
        }
    }

    /* Reset socket to be blocking after connect(2). */
    if (blocking && nexcacheSetBlocking(c, 1) != NEXCACHE_OK)
        return NEXCACHE_ERR;

    c->flags |= NEXCACHE_CONNECTED;
    return NEXCACHE_OK;
#else
    /* We currently do not support Unix sockets for Windows. */
    /* TODO(m): https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/ */
    errno = EPROTONOSUPPORT;
    return NEXCACHE_ERR;
#endif /* _WIN32 */
oom:
    nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
    return NEXCACHE_ERR;
}

static nexcacheContextFuncs nexcacheContextTcpFuncs = {
    .connect = nexcacheContextConnectTcp,
    .close = nexcacheNetClose,
    .free_privctx = NULL,
    .async_read = nexcacheAsyncRead,
    .async_write = nexcacheAsyncWrite,
    .read = nexcacheNetRead,
    .write = nexcacheNetWrite,
    .set_timeout = nexcacheTcpSetTimeout,
};

void nexcacheContextRegisterTcpFuncs(void) {
    nexcacheContextRegisterFuncs(&nexcacheContextTcpFuncs, NEXCACHE_CONN_TCP);
}

static nexcacheContextFuncs nexcacheContextUnixFuncs = {
    .connect = nexcacheContextConnectUnix,
    .close = nexcacheNetClose,
    .free_privctx = NULL,
    .async_read = nexcacheAsyncRead,
    .async_write = nexcacheAsyncWrite,
    .read = nexcacheNetRead,
    .write = nexcacheNetWrite,
    .set_timeout = nexcacheTcpSetTimeout,
};

void nexcacheContextRegisterUnixFuncs(void) {
    nexcacheContextRegisterFuncs(&nexcacheContextUnixFuncs, NEXCACHE_CONN_UNIX);
}

static int nexcacheContextConnectUserfd(nexcacheContext *c, const nexcacheOptions *options) {
    c->fd = options->endpoint.fd;
    c->flags |= NEXCACHE_CONNECTED;

    return NEXCACHE_OK;
}

static nexcacheContextFuncs nexcacheContextUserfdFuncs = {
    .connect = nexcacheContextConnectUserfd,
    .close = nexcacheNetClose,
    .free_privctx = NULL,
    .async_read = nexcacheAsyncRead,
    .async_write = nexcacheAsyncWrite,
    .read = nexcacheNetRead,
    .write = nexcacheNetWrite,
    .set_timeout = nexcacheTcpSetTimeout,
};

void nexcacheContextRegisterUserfdFuncs(void) {
    nexcacheContextRegisterFuncs(&nexcacheContextUserfdFuncs, NEXCACHE_CONN_USERFD);
}
