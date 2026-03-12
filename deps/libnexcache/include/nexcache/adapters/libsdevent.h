#ifndef NEXCACHE_ADAPTERS_LIBSDEVENT_H
#define NEXCACHE_ADAPTERS_LIBSDEVENT_H
#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <systemd/sd-event.h>

#define NEXCACHE_LIBSDEVENT_DELETED 0x01
#define NEXCACHE_LIBSDEVENT_ENTERED 0x02

typedef struct nexcacheLibsdeventEvents {
    nexcacheAsyncContext *context;
    struct sd_event *event;
    struct sd_event_source *fdSource;
    struct sd_event_source *timerSource;
    int fd;
    short flags;
    short state;
} nexcacheLibsdeventEvents;

static void nexcacheLibsdeventDestroy(nexcacheLibsdeventEvents *e) {
    if (e->fdSource) {
        e->fdSource = sd_event_source_disable_unref(e->fdSource);
    }
    if (e->timerSource) {
        e->timerSource = sd_event_source_disable_unref(e->timerSource);
    }
    sd_event_unref(e->event);
    vk_free(e);
}

static int nexcacheLibsdeventTimeoutHandler(sd_event_source *s, uint64_t usec, void *userdata) {
    ((void)s);
    ((void)usec);
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;
    nexcacheAsyncHandleTimeout(e->context);
    return 0;
}

static int nexcacheLibsdeventHandler(sd_event_source *s, int fd, uint32_t event, void *userdata) {
    ((void)s);
    ((void)fd);
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;
    e->state |= NEXCACHE_LIBSDEVENT_ENTERED;

#define CHECK_DELETED()                         \
    if (e->state & NEXCACHE_LIBSDEVENT_DELETED) { \
        nexcacheLibsdeventDestroy(e);             \
        return 0;                               \
    }

    if ((event & EPOLLIN) && e->context && (e->state & NEXCACHE_LIBSDEVENT_DELETED) == 0) {
        nexcacheAsyncHandleRead(e->context);
        CHECK_DELETED();
    }

    if ((event & EPOLLOUT) && e->context && (e->state & NEXCACHE_LIBSDEVENT_DELETED) == 0) {
        nexcacheAsyncHandleWrite(e->context);
        CHECK_DELETED();
    }

    e->state &= ~NEXCACHE_LIBSDEVENT_ENTERED;
#undef CHECK_DELETED

    return 0;
}

static void nexcacheLibsdeventAddRead(void *userdata) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    if (e->flags & EPOLLIN) {
        return;
    }

    e->flags |= EPOLLIN;

    if (e->flags & EPOLLOUT) {
        sd_event_source_set_io_events(e->fdSource, e->flags);
    } else {
        sd_event_add_io(e->event, &e->fdSource, e->fd, e->flags, nexcacheLibsdeventHandler, e);
    }
}

static void nexcacheLibsdeventDelRead(void *userdata) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    e->flags &= ~EPOLLIN;

    if (e->flags) {
        sd_event_source_set_io_events(e->fdSource, e->flags);
    } else {
        e->fdSource = sd_event_source_disable_unref(e->fdSource);
    }
}

static void nexcacheLibsdeventAddWrite(void *userdata) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    if (e->flags & EPOLLOUT) {
        return;
    }

    e->flags |= EPOLLOUT;

    if (e->flags & EPOLLIN) {
        sd_event_source_set_io_events(e->fdSource, e->flags);
    } else {
        sd_event_add_io(e->event, &e->fdSource, e->fd, e->flags, nexcacheLibsdeventHandler, e);
    }
}

static void nexcacheLibsdeventDelWrite(void *userdata) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    e->flags &= ~EPOLLOUT;

    if (e->flags) {
        sd_event_source_set_io_events(e->fdSource, e->flags);
    } else {
        e->fdSource = sd_event_source_disable_unref(e->fdSource);
    }
}

static void nexcacheLibsdeventCleanup(void *userdata) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    if (!e) {
        return;
    }

    if (e->state & NEXCACHE_LIBSDEVENT_ENTERED) {
        e->state |= NEXCACHE_LIBSDEVENT_DELETED;
    } else {
        nexcacheLibsdeventDestroy(e);
    }
}

static void nexcacheLibsdeventSetTimeout(void *userdata, struct timeval tv) {
    nexcacheLibsdeventEvents *e = (nexcacheLibsdeventEvents *)userdata;

    uint64_t usec = tv.tv_sec * 1000000 + tv.tv_usec;
    if (!e->timerSource) {
        sd_event_add_time_relative(e->event, &e->timerSource, CLOCK_MONOTONIC, usec, 1, nexcacheLibsdeventTimeoutHandler, e);
    } else {
        sd_event_source_set_time_relative(e->timerSource, usec);
    }
}

static int nexcacheLibsdeventAttach(nexcacheAsyncContext *ac, struct sd_event *event) {
    nexcacheContext *c = &(ac->c);
    nexcacheLibsdeventEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcacheLibsdeventEvents *)vk_calloc(1, sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;

    /* Initialize and increase event refcount */
    e->context = ac;
    e->event = event;
    e->fd = c->fd;
    sd_event_ref(event);

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcacheLibsdeventAddRead;
    ac->ev.delRead = nexcacheLibsdeventDelRead;
    ac->ev.addWrite = nexcacheLibsdeventAddWrite;
    ac->ev.delWrite = nexcacheLibsdeventDelWrite;
    ac->ev.cleanup = nexcacheLibsdeventCleanup;
    ac->ev.scheduleTimer = nexcacheLibsdeventSetTimeout;
    ac->ev.data = e;

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheLibsdeventAttachAdapter(nexcacheAsyncContext *ac, void *event) {
    return nexcacheLibsdeventAttach(ac, (struct sd_event *)event);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseLibsdevent(nexcacheClusterOptions *options,
                                             struct sd_event *event) {
    if (options == NULL || event == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheLibsdeventAttachAdapter;
    options->attach_data = event;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_LIBSDEVENT_H */
