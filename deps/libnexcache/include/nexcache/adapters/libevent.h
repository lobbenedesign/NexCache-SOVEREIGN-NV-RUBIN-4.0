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

#ifndef NEXCACHE_ADAPTERS_LIBEVENT_H
#define NEXCACHE_ADAPTERS_LIBEVENT_H
#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <event2/event.h>

#define NEXCACHE_LIBEVENT_DELETED 0x01
#define NEXCACHE_LIBEVENT_ENTERED 0x02

typedef struct nexcacheLibeventEvents {
    nexcacheAsyncContext *context;
    struct event *ev;
    struct event_base *base;
    struct timeval tv;
    short flags;
    short state;
} nexcacheLibeventEvents;

static void nexcacheLibeventDestroy(nexcacheLibeventEvents *e) {
    vk_free(e);
}

static void nexcacheLibeventHandler(evutil_socket_t fd, short event, void *arg) {
    ((void)fd);
    nexcacheLibeventEvents *e = (nexcacheLibeventEvents *)arg;
    e->state |= NEXCACHE_LIBEVENT_ENTERED;

#define CHECK_DELETED()                       \
    if (e->state & NEXCACHE_LIBEVENT_DELETED) { \
        nexcacheLibeventDestroy(e);             \
        return;                               \
    }

    if ((event & EV_TIMEOUT) && (e->state & NEXCACHE_LIBEVENT_DELETED) == 0) {
        nexcacheAsyncHandleTimeout(e->context);
        CHECK_DELETED();
    }

    if ((event & EV_READ) && e->context && (e->state & NEXCACHE_LIBEVENT_DELETED) == 0) {
        nexcacheAsyncHandleRead(e->context);
        CHECK_DELETED();
    }

    if ((event & EV_WRITE) && e->context && (e->state & NEXCACHE_LIBEVENT_DELETED) == 0) {
        nexcacheAsyncHandleWrite(e->context);
        CHECK_DELETED();
    }

    e->state &= ~NEXCACHE_LIBEVENT_ENTERED;
#undef CHECK_DELETED
}

static void nexcacheLibeventUpdate(void *privdata, short flag, int isRemove) {
    nexcacheLibeventEvents *e = (nexcacheLibeventEvents *)privdata;
    const struct timeval *tv = e->tv.tv_sec || e->tv.tv_usec ? &e->tv : NULL;

    if (isRemove) {
        if ((e->flags & flag) == 0) {
            return;
        } else {
            e->flags &= ~flag;
        }
    } else {
        if (e->flags & flag) {
            return;
        } else {
            e->flags |= flag;
        }
    }

    event_del(e->ev);
    event_assign(e->ev, e->base, e->context->c.fd, e->flags | EV_PERSIST,
                 nexcacheLibeventHandler, privdata);
    event_add(e->ev, tv);
}

static void nexcacheLibeventAddRead(void *privdata) {
    nexcacheLibeventUpdate(privdata, EV_READ, 0);
}

static void nexcacheLibeventDelRead(void *privdata) {
    nexcacheLibeventUpdate(privdata, EV_READ, 1);
}

static void nexcacheLibeventAddWrite(void *privdata) {
    nexcacheLibeventUpdate(privdata, EV_WRITE, 0);
}

static void nexcacheLibeventDelWrite(void *privdata) {
    nexcacheLibeventUpdate(privdata, EV_WRITE, 1);
}

static void nexcacheLibeventCleanup(void *privdata) {
    nexcacheLibeventEvents *e = (nexcacheLibeventEvents *)privdata;
    if (!e) {
        return;
    }
    event_del(e->ev);
    event_free(e->ev);
    e->ev = NULL;

    if (e->state & NEXCACHE_LIBEVENT_ENTERED) {
        e->state |= NEXCACHE_LIBEVENT_DELETED;
    } else {
        nexcacheLibeventDestroy(e);
    }
}

static void nexcacheLibeventSetTimeout(void *privdata, struct timeval tv) {
    nexcacheLibeventEvents *e = (nexcacheLibeventEvents *)privdata;
    short flags = e->flags;
    e->flags = 0;
    e->tv = tv;
    nexcacheLibeventUpdate(e, flags, 0);
}

static int nexcacheLibeventAttach(nexcacheAsyncContext *ac, struct event_base *base) {
    nexcacheContext *c = &(ac->c);
    nexcacheLibeventEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcacheLibeventEvents *)vk_calloc(1, sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;

    e->context = ac;

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcacheLibeventAddRead;
    ac->ev.delRead = nexcacheLibeventDelRead;
    ac->ev.addWrite = nexcacheLibeventAddWrite;
    ac->ev.delWrite = nexcacheLibeventDelWrite;
    ac->ev.cleanup = nexcacheLibeventCleanup;
    ac->ev.scheduleTimer = nexcacheLibeventSetTimeout;
    ac->ev.data = e;

    /* Initialize and install read/write events */
    e->ev = event_new(base, c->fd, EV_READ | EV_WRITE, nexcacheLibeventHandler, e);
    e->base = base;
    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheLibeventAttachAdapter(nexcacheAsyncContext *ac, void *base) {
    return nexcacheLibeventAttach(ac, (struct event_base *)base);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseLibevent(nexcacheClusterOptions *options,
                                           struct event_base *base) {
    if (options == NULL || base == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheLibeventAttachAdapter;
    options->attach_data = base;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_LIBEVENT_H */
