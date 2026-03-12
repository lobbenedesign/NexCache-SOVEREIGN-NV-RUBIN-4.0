#ifndef NEXCACHE_ADAPTERS_IVYKIS_H
#define NEXCACHE_ADAPTERS_IVYKIS_H
#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <iv.h>

typedef struct nexcacheIvykisEvents {
    nexcacheAsyncContext *context;
    struct iv_fd fd;
} nexcacheIvykisEvents;

static void nexcacheIvykisReadEvent(void *arg) {
    nexcacheAsyncContext *context = (nexcacheAsyncContext *)arg;
    nexcacheAsyncHandleRead(context);
}

static void nexcacheIvykisWriteEvent(void *arg) {
    nexcacheAsyncContext *context = (nexcacheAsyncContext *)arg;
    nexcacheAsyncHandleWrite(context);
}

static void nexcacheIvykisAddRead(void *privdata) {
    nexcacheIvykisEvents *e = (nexcacheIvykisEvents *)privdata;
    iv_fd_set_handler_in(&e->fd, nexcacheIvykisReadEvent);
}

static void nexcacheIvykisDelRead(void *privdata) {
    nexcacheIvykisEvents *e = (nexcacheIvykisEvents *)privdata;
    iv_fd_set_handler_in(&e->fd, NULL);
}

static void nexcacheIvykisAddWrite(void *privdata) {
    nexcacheIvykisEvents *e = (nexcacheIvykisEvents *)privdata;
    iv_fd_set_handler_out(&e->fd, nexcacheIvykisWriteEvent);
}

static void nexcacheIvykisDelWrite(void *privdata) {
    nexcacheIvykisEvents *e = (nexcacheIvykisEvents *)privdata;
    iv_fd_set_handler_out(&e->fd, NULL);
}

static void nexcacheIvykisCleanup(void *privdata) {
    nexcacheIvykisEvents *e = (nexcacheIvykisEvents *)privdata;

    iv_fd_unregister(&e->fd);
    vk_free(e);
}

static int nexcacheIvykisAttach(nexcacheAsyncContext *ac) {
    nexcacheContext *c = &(ac->c);
    nexcacheIvykisEvents *e;

    /* Nothing should be attached when something is already attached */
    if (ac->ev.data != NULL)
        return NEXCACHE_ERR;

    /* Create container for context and r/w events */
    e = (nexcacheIvykisEvents *)vk_malloc(sizeof(*e));
    if (e == NULL)
        return NEXCACHE_ERR;

    e->context = ac;

    /* Register functions to start/stop listening for events */
    ac->ev.addRead = nexcacheIvykisAddRead;
    ac->ev.delRead = nexcacheIvykisDelRead;
    ac->ev.addWrite = nexcacheIvykisAddWrite;
    ac->ev.delWrite = nexcacheIvykisDelWrite;
    ac->ev.cleanup = nexcacheIvykisCleanup;
    ac->ev.data = e;

    /* Initialize and install read/write events */
    IV_FD_INIT(&e->fd);
    e->fd.fd = c->fd;
    e->fd.handler_in = nexcacheIvykisReadEvent;
    e->fd.handler_out = nexcacheIvykisWriteEvent;
    e->fd.handler_err = NULL;
    e->fd.cookie = e->context;

    iv_fd_register(&e->fd);

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheIvykisAttachAdapter(nexcacheAsyncContext *ac, NEXCACHE_UNUSED void *) {
    return nexcacheIvykisAttach(ac);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseIvykis(nexcacheClusterOptions *options) {
    if (options == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheIvykisAttachAdapter;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_IVYKIS_H */
