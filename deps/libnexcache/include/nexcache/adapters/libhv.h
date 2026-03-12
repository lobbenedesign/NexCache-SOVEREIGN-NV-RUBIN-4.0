#ifndef NEXCACHE_ADAPTERS_LIBHV_H
#define NEXCACHE_ADAPTERS_LIBHV_H

#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <hv/hloop.h>

typedef struct nexcacheLibhvEvents {
    hio_t *io;
    htimer_t *timer;
} nexcacheLibhvEvents;

static void nexcacheLibhvHandleEvents(hio_t *io) {
    nexcacheAsyncContext *context = (nexcacheAsyncContext *)hevent_userdata(io);
    int events = hio_events(io);
    int revents = hio_revents(io);
    if (context && (events & HV_READ) && (revents & HV_READ)) {
        nexcacheAsyncHandleRead(context);
    }
    if (context && (events & HV_WRITE) && (revents & HV_WRITE)) {
        nexcacheAsyncHandleWrite(context);
    }
}

static void nexcacheLibhvAddRead(void *privdata) {
    nexcacheLibhvEvents *events = (nexcacheLibhvEvents *)privdata;
    hio_add(events->io, nexcacheLibhvHandleEvents, HV_READ);
}

static void nexcacheLibhvDelRead(void *privdata) {
    nexcacheLibhvEvents *events = (nexcacheLibhvEvents *)privdata;
    hio_del(events->io, HV_READ);
}

static void nexcacheLibhvAddWrite(void *privdata) {
    nexcacheLibhvEvents *events = (nexcacheLibhvEvents *)privdata;
    hio_add(events->io, nexcacheLibhvHandleEvents, HV_WRITE);
}

static void nexcacheLibhvDelWrite(void *privdata) {
    nexcacheLibhvEvents *events = (nexcacheLibhvEvents *)privdata;
    hio_del(events->io, HV_WRITE);
}

static void nexcacheLibhvCleanup(void *privdata) {
    nexcacheLibhvEvents *events = (nexcacheLibhvEvents *)privdata;

    if (events->timer)
        htimer_del(events->timer);

    hio_close(events->io);
    hevent_set_userdata(events->io, NULL);

    vk_free(events);
}

static void nexcacheLibhvTimeout(htimer_t *timer) {
    hio_t *io = (hio_t *)hevent_userdata(timer);
    nexcacheAsyncHandleTimeout((nexcacheAsyncContext *)hevent_userdata(io));
}

static void nexcacheLibhvSetTimeout(void *privdata, struct timeval tv) {
    nexcacheLibhvEvents *events;
    uint32_t millis;
    hloop_t *loop;

    events = (nexcacheLibhvEvents *)privdata;
    millis = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    if (millis == 0) {
        /* Libhv disallows zero'd timers so treat this as a delete or NO OP */
        if (events->timer) {
            htimer_del(events->timer);
            events->timer = NULL;
        }
    } else if (events->timer == NULL) {
        /* Add new timer */
        loop = hevent_loop(events->io);
        events->timer = htimer_add(loop, nexcacheLibhvTimeout, millis, 1);
        hevent_set_userdata(events->timer, events->io);
    } else {
        /* Update existing timer */
        htimer_reset(events->timer, millis);
    }
}

static int nexcacheLibhvAttach(nexcacheAsyncContext *ac, hloop_t *loop) {
    nexcacheContext *c = &(ac->c);
    nexcacheLibhvEvents *events;
    hio_t *io = NULL;

    if (ac->ev.data != NULL) {
        return NEXCACHE_ERR;
    }

    /* Create container struct to keep track of our io and any timer */
    events = (nexcacheLibhvEvents *)vk_malloc(sizeof(*events));
    if (events == NULL) {
        return NEXCACHE_ERR;
    }

    io = hio_get(loop, c->fd);
    if (io == NULL) {
        vk_free(events);
        return NEXCACHE_ERR;
    }

    hevent_set_userdata(io, ac);

    events->io = io;
    events->timer = NULL;

    ac->ev.addRead = nexcacheLibhvAddRead;
    ac->ev.delRead = nexcacheLibhvDelRead;
    ac->ev.addWrite = nexcacheLibhvAddWrite;
    ac->ev.delWrite = nexcacheLibhvDelWrite;
    ac->ev.cleanup = nexcacheLibhvCleanup;
    ac->ev.scheduleTimer = nexcacheLibhvSetTimeout;
    ac->ev.data = events;

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheLibhvAttachAdapter(nexcacheAsyncContext *ac, void *loop) {
    return nexcacheLibhvAttach(ac, (hloop_t *)loop);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseLibhv(nexcacheClusterOptions *options,
                                        hloop_t *loop) {
    if (options == NULL || loop == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheLibhvAttachAdapter;
    options->attach_data = loop;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_LIBHV_H */
