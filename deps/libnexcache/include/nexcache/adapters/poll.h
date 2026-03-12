
#ifndef NEXCACHE_ADAPTERS_POLL_H
#define NEXCACHE_ADAPTERS_POLL_H

#include "../async.h"
#include "../cluster.h"
#include "../sockcompat.h"

#include <errno.h>
#include <string.h> // for memset

/* Values to return from nexcachePollTick */
#define NEXCACHE_POLL_HANDLED_READ 1
#define NEXCACHE_POLL_HANDLED_WRITE 2
#define NEXCACHE_POLL_HANDLED_TIMEOUT 4

/* An adapter to allow manual polling of the async context by checking the state
 * of the underlying file descriptor.  Useful in cases where there is no formal
 * IO event loop but regular ticking can be used, such as in game engines. */

typedef struct nexcachePollEvents {
    nexcacheAsyncContext *context;
    nexcacheFD fd;
    char reading, writing;
    char in_tick;
    char deleted;
    double deadline;
} nexcachePollEvents;

static double nexcachePollTimevalToDouble(struct timeval *tv) {
    if (tv == NULL)
        return 0.0;
    return tv->tv_sec + tv->tv_usec / 1000000.00;
}

static double nexcachePollGetNow(void) {
#ifndef _MSC_VER
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return nexcachePollTimevalToDouble(&tv);
#else
    FILETIME ft;
    ULARGE_INTEGER li;
    GetSystemTimeAsFileTime(&ft);
    li.HighPart = ft.dwHighDateTime;
    li.LowPart = ft.dwLowDateTime;
    return (double)li.QuadPart * 1e-7;
#endif
}

/* Poll for io, handling any pending callbacks.  The timeout argument can be
 * positive to wait for a maximum given time for IO, zero to poll, or negative
 * to wait forever */
static int nexcachePollTick(nexcacheAsyncContext *ac, double timeout) {
    int reading, writing;
    struct pollfd pfd;
    int handled;
    int ns;
    int itimeout;

    nexcachePollEvents *e = (nexcachePollEvents *)ac->ev.data;
    if (!e)
        return 0;

    /* local flags, won't get changed during callbacks */
    reading = e->reading;
    writing = e->writing;
    if (!reading && !writing)
        return 0;

    pfd.fd = e->fd;
    pfd.events = 0;
    if (reading)
        pfd.events = POLLIN;
    if (writing)
        pfd.events |= POLLOUT;

    if (timeout >= 0.0) {
        itimeout = (int)(timeout * 1000.0);
    } else {
        itimeout = -1;
    }

    ns = poll(&pfd, 1, itimeout);
    if (ns < 0) {
        /* ignore the EINTR error */
        if (errno != EINTR)
            return ns;
        ns = 0;
    }

    handled = 0;
    e->in_tick = 1;
    if (ns) {
        if (reading && (pfd.revents & POLLIN)) {
            nexcacheAsyncHandleRead(ac);
            handled |= NEXCACHE_POLL_HANDLED_READ;
        }
        /* on Windows, connection failure is indicated with the Exception fdset.
         * handle it the same as writable. */
        if (writing && (pfd.revents & (POLLOUT | POLLERR))) {
            /* context Read callback may have caused context to be deleted, e.g.
               by doing a nexcacheAsyncDisconnect() */
            if (!e->deleted) {
                nexcacheAsyncHandleWrite(ac);
                handled |= NEXCACHE_POLL_HANDLED_WRITE;
            }
        }
    }

    /* perform timeouts */
    if (!e->deleted && e->deadline != 0.0) {
        double now = nexcachePollGetNow();
        if (now >= e->deadline) {
            /* deadline has passed.  disable timeout and perform callback */
            e->deadline = 0.0;
            nexcacheAsyncHandleTimeout(ac);
            handled |= NEXCACHE_POLL_HANDLED_TIMEOUT;
        }
    }

    /* do a delayed cleanup if required */
    if (e->deleted)
        vk_free(e);
    else
        e->in_tick = 0;

    return handled;
}

static void nexcachePollAddRead(void *data) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;
    e->reading = 1;
}

static void nexcachePollDelRead(void *data) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;
    e->reading = 0;
}

static void nexcachePollAddWrite(void *data) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;
    e->writing = 1;
}

static void nexcachePollDelWrite(void *data) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;
    e->writing = 0;
}

static void nexcachePollCleanup(void *data) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;

    /* if we are currently processing a tick, postpone deletion */
    if (e->in_tick)
        e->deleted = 1;
    else
        vk_free(e);
}

static void nexcachePollScheduleTimer(void *data, struct timeval tv) {
    nexcachePollEvents *e = (nexcachePollEvents *)data;
    double now = nexcachePollGetNow();
    e->deadline = now + nexcachePollTimevalToDouble(&tv);
}

static int nexcachePollAttach(nexcacheAsyncContext *ac) {
    nexcacheContext *c = &(ac->c);
    nexcachePollEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcachePollEvents *)vk_malloc(sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;
    memset(e, 0, sizeof(*e));

    e->context = ac;
    e->fd = c->fd;
    e->reading = e->writing = 0;
    e->in_tick = e->deleted = 0;
    e->deadline = 0.0;

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcachePollAddRead;
    ac->ev.delRead = nexcachePollDelRead;
    ac->ev.addWrite = nexcachePollAddWrite;
    ac->ev.delWrite = nexcachePollDelWrite;
    ac->ev.scheduleTimer = nexcachePollScheduleTimer;
    ac->ev.cleanup = nexcachePollCleanup;
    ac->ev.data = e;

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcachePollAttachAdapter(nexcacheAsyncContext *ac, NEXCACHE_UNUSED void *unused) {
    return nexcachePollAttach(ac);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUsePoll(nexcacheClusterOptions *options) {
    if (options == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcachePollAttachAdapter;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_POLL_H */
