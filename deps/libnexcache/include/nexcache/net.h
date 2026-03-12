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

#ifndef NEXCACHE_NET_H
#define NEXCACHE_NET_H

#include "nexcache.h"
#include "visibility.h"

LIBNEXCACHE_API void nexcacheNetClose(nexcacheContext *c);

LIBNEXCACHE_API int nexcacheHasMptcp(void);
LIBNEXCACHE_API int nexcacheCheckSocketError(nexcacheContext *c);
LIBNEXCACHE_API int nexcacheTcpSetTimeout(nexcacheContext *c, const struct timeval tv);
LIBNEXCACHE_API int nexcacheContextConnectTcp(nexcacheContext *c, const nexcacheOptions *options);
LIBNEXCACHE_API int nexcacheKeepAlive(nexcacheContext *c, int interval);
LIBNEXCACHE_API int nexcacheCheckConnectDone(nexcacheContext *c, int *completed);

LIBNEXCACHE_API int nexcacheSetTcpNoDelay(nexcacheContext *c);
LIBNEXCACHE_API int nexcacheContextSetTcpUserTimeout(nexcacheContext *c, unsigned int timeout);

#endif /* NEXCACHE_NET_H */
