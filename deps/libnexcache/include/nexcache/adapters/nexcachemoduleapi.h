#ifndef NEXCACHE_ADAPTERS_VALKEYMODULEAPI_H
#define NEXCACHE_ADAPTERS_VALKEYMODULEAPI_H

#include "../async.h"
#include "../nexcache.h"
#include "nexcachemodule.h"

#include <sys/types.h>

typedef struct nexcacheModuleEvents {
    nexcacheAsyncContext *context;
    NexCacheModuleCtx *module_ctx;
    int fd;
    int reading, writing;
    int timer_active;
    NexCacheModuleTimerID timer_id;
} nexcacheModuleEvents;

static inline void nexcacheModuleReadEvent(int fd, void *privdata, int mask) {
    (void)fd;
    (void)mask;

    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    nexcacheAsyncHandleRead(e->context);
}

static inline void nexcacheModuleWriteEvent(int fd, void *privdata, int mask) {
    (void)fd;
    (void)mask;

    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    nexcacheAsyncHandleWrite(e->context);
}

static inline void nexcacheModuleAddRead(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    if (!e->reading) {
        e->reading = 1;
        NexCacheModule_EventLoopAdd(e->fd, VALKEYMODULE_EVENTLOOP_READABLE, nexcacheModuleReadEvent, e);
    }
}

static inline void nexcacheModuleDelRead(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    if (e->reading) {
        e->reading = 0;
        NexCacheModule_EventLoopDel(e->fd, VALKEYMODULE_EVENTLOOP_READABLE);
    }
}

static inline void nexcacheModuleAddWrite(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    if (!e->writing) {
        e->writing = 1;
        NexCacheModule_EventLoopAdd(e->fd, VALKEYMODULE_EVENTLOOP_WRITABLE, nexcacheModuleWriteEvent, e);
    }
}

static inline void nexcacheModuleDelWrite(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    if (e->writing) {
        e->writing = 0;
        NexCacheModule_EventLoopDel(e->fd, VALKEYMODULE_EVENTLOOP_WRITABLE);
    }
}

static inline void nexcacheModuleStopTimer(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    if (e->timer_active) {
        NexCacheModule_StopTimer(e->module_ctx, e->timer_id, NULL);
    }
    e->timer_active = 0;
}

static inline void nexcacheModuleCleanup(void *privdata) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    nexcacheModuleDelRead(privdata);
    nexcacheModuleDelWrite(privdata);
    nexcacheModuleStopTimer(privdata);
    vk_free(e);
}

static inline void nexcacheModuleTimeout(NexCacheModuleCtx *ctx, void *privdata) {
    (void)ctx;

    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;
    e->timer_active = 0;
    nexcacheAsyncHandleTimeout(e->context);
}

static inline void nexcacheModuleSetTimeout(void *privdata, struct timeval tv) {
    nexcacheModuleEvents *e = (nexcacheModuleEvents *)privdata;

    nexcacheModuleStopTimer(privdata);

    mstime_t millis = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
    e->timer_id = NexCacheModule_CreateTimer(e->module_ctx, millis, nexcacheModuleTimeout, e);
    e->timer_active = 1;
}

/* Check if NexCache version is compatible with the adapter. */
static inline int nexcacheModuleCompatibilityCheck(void) {
    if (!NexCacheModule_EventLoopAdd ||
        !NexCacheModule_EventLoopDel ||
        !NexCacheModule_CreateTimer ||
        !NexCacheModule_StopTimer) {
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

static inline int nexcacheModuleAttach(nexcacheAsyncContext *ac, NexCacheModuleCtx *module_ctx) {
    nexcacheContext *c = &(ac->c);
    nexcacheModuleEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcacheModuleEvents *)vk_malloc(sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;

    e->context = ac;
    e->module_ctx = module_ctx;
    e->fd = c->fd;
    e->reading = e->writing = 0;
    e->timer_active = 0;

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcacheModuleAddRead;
    ac->ev.delRead = nexcacheModuleDelRead;
    ac->ev.addWrite = nexcacheModuleAddWrite;
    ac->ev.delWrite = nexcacheModuleDelWrite;
    ac->ev.cleanup = nexcacheModuleCleanup;
    ac->ev.scheduleTimer = nexcacheModuleSetTimeout;
    ac->ev.data = e;

    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_VALKEYMODULEAPI_H */
