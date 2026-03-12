/*
 * Copyright (c) 2024, zhenwei pi <pizhenwei@bytedance.com>
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
 *   * Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef NEXCACHE_VK_PRIVATE_H
#define NEXCACHE_VK_PRIVATE_H

#include "win32.h"

#include "nexcache.h"
#include "visibility.h"

#include <sds.h>

#include <limits.h>
#include <string.h>

LIBNEXCACHE_API void nexcacheSetError(nexcacheContext *c, int type, const char *str);

/* Helper function. Convert struct timeval to millisecond. */
static inline int nexcacheContextTimeoutMsec(const struct timeval *timeout, long *result) {
    long max_msec = (LONG_MAX - 999) / 1000;
    long msec = INT_MAX;

    /* Only use timeout when not NULL. */
    if (timeout != NULL) {
        if (timeout->tv_usec > 1000000 || timeout->tv_sec > max_msec) {
            *result = msec;
            return NEXCACHE_ERR;
        }

        msec = (timeout->tv_sec * 1000) + ((timeout->tv_usec + 999) / 1000);

        if (msec < 0 || msec > INT_MAX) {
            msec = INT_MAX;
        }
    }

    *result = msec;
    return NEXCACHE_OK;
}

/* Get connect timeout of nexcacheContext */
static inline int nexcacheConnectTimeoutMsec(nexcacheContext *c, long *result) {
    const struct timeval *timeout = c->connect_timeout;
    int ret = nexcacheContextTimeoutMsec(timeout, result);

    if (ret != NEXCACHE_OK) {
        nexcacheSetError(c, NEXCACHE_ERR_IO, "Invalid timeout specified");
    }

    return ret;
}

/* Get command timeout of nexcacheContext */
static inline int nexcacheCommandTimeoutMsec(nexcacheContext *c, long *result) {
    const struct timeval *timeout = c->command_timeout;
    int ret = nexcacheContextTimeoutMsec(timeout, result);

    if (ret != NEXCACHE_OK) {
        nexcacheSetError(c, NEXCACHE_ERR_IO, "Invalid timeout specified");
    }

    return ret;
}

static inline int nexcacheContextUpdateConnectTimeout(nexcacheContext *c,
                                                    const struct timeval *timeout) {
    /* Same timeval struct, short circuit */
    if (c->connect_timeout == timeout)
        return NEXCACHE_OK;

    /* Allocate context timeval if we need to */
    if (c->connect_timeout == NULL) {
        c->connect_timeout = vk_malloc(sizeof(*c->connect_timeout));
        if (c->connect_timeout == NULL)
            return NEXCACHE_ERR;
    }

    memcpy(c->connect_timeout, timeout, sizeof(*c->connect_timeout));
    return NEXCACHE_OK;
}

static inline int nexcacheContextUpdateCommandTimeout(nexcacheContext *c,
                                                    const struct timeval *timeout) {
    /* Same timeval struct, short circuit */
    if (c->command_timeout == timeout)
        return NEXCACHE_OK;

    /* Allocate context timeval if we need to */
    if (c->command_timeout == NULL) {
        c->command_timeout = vk_malloc(sizeof(*c->command_timeout));
        if (c->command_timeout == NULL)
            return NEXCACHE_ERR;
    }

    memcpy(c->command_timeout, timeout, sizeof(*c->command_timeout));
    return NEXCACHE_OK;
}

/* Visible although private since required by libnexcache_rdma.so */
LIBNEXCACHE_API int nexcacheContextRegisterFuncs(nexcacheContextFuncs *funcs, enum nexcacheConnectionType type);
void nexcacheContextRegisterTcpFuncs(void);
void nexcacheContextRegisterUnixFuncs(void);
void nexcacheContextRegisterUserfdFuncs(void);

void nexcacheContextSetFuncs(nexcacheContext *c);

long long nexcacheFormatSdsCommandArgv(sds *target, int argc, const char **argv, const size_t *argvlen);

#endif /* NEXCACHE_VK_PRIVATE_H */
