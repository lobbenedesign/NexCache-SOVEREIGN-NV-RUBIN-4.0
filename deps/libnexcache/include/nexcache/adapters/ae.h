/*
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

#ifndef NEXCACHE_ADAPTERS_AE_H
#define NEXCACHE_ADAPTERS_AE_H
#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <ae.h>
#include <sys/types.h>

typedef struct nexcacheAeEvents {
    nexcacheAsyncContext *context;
    aeEventLoop *loop;
    int fd;
    int reading, writing;
} nexcacheAeEvents;

static void nexcacheAeReadEvent(aeEventLoop *el, int fd, void *privdata, int mask) {
    ((void)el);
    ((void)fd);
    ((void)mask);

    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    nexcacheAsyncHandleRead(e->context);
}

static void nexcacheAeWriteEvent(aeEventLoop *el, int fd, void *privdata, int mask) {
    ((void)el);
    ((void)fd);
    ((void)mask);

    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    nexcacheAsyncHandleWrite(e->context);
}

static void nexcacheAeAddRead(void *privdata) {
    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    aeEventLoop *loop = e->loop;
    if (!e->reading) {
        e->reading = 1;
        aeCreateFileEvent(loop, e->fd, AE_READABLE, nexcacheAeReadEvent, e);
    }
}

static void nexcacheAeDelRead(void *privdata) {
    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    aeEventLoop *loop = e->loop;
    if (e->reading) {
        e->reading = 0;
        aeDeleteFileEvent(loop, e->fd, AE_READABLE);
    }
}

static void nexcacheAeAddWrite(void *privdata) {
    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    aeEventLoop *loop = e->loop;
    if (!e->writing) {
        e->writing = 1;
        aeCreateFileEvent(loop, e->fd, AE_WRITABLE, nexcacheAeWriteEvent, e);
    }
}

static void nexcacheAeDelWrite(void *privdata) {
    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    aeEventLoop *loop = e->loop;
    if (e->writing) {
        e->writing = 0;
        aeDeleteFileEvent(loop, e->fd, AE_WRITABLE);
    }
}

static void nexcacheAeCleanup(void *privdata) {
    nexcacheAeEvents *e = (nexcacheAeEvents *)privdata;
    nexcacheAeDelRead(privdata);
    nexcacheAeDelWrite(privdata);
    vk_free(e);
}

static int nexcacheAeAttach(aeEventLoop *loop, nexcacheAsyncContext *ac) {
    nexcacheContext *c = &(ac->c);
    nexcacheAeEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcacheAeEvents *)vk_malloc(sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;

    e->context = ac;
    e->loop = loop;
    e->fd = c->fd;
    e->reading = e->writing = 0;

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcacheAeAddRead;
    ac->ev.delRead = nexcacheAeDelRead;
    ac->ev.addWrite = nexcacheAeAddWrite;
    ac->ev.delWrite = nexcacheAeDelWrite;
    ac->ev.cleanup = nexcacheAeCleanup;
    ac->ev.data = e;

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheAeAttachAdapter(nexcacheAsyncContext *ac, void *loop) {
    return nexcacheAeAttach((aeEventLoop *)loop, ac);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseAe(nexcacheClusterOptions *options,
                                     aeEventLoop *loop) {
    if (options == NULL || loop == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheAeAttachAdapter;
    options->attach_data = loop;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_AE_H */
