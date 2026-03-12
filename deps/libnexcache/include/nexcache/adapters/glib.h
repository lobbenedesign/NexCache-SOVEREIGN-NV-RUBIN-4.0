#ifndef NEXCACHE_ADAPTERS_GLIB_H
#define NEXCACHE_ADAPTERS_GLIB_H

#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <glib.h>

typedef struct
{
    GSource source;
    nexcacheAsyncContext *ac;
    GPollFD poll_fd;
} NexCacheSource;

static void
nexcache_source_add_read(gpointer data) {
    NexCacheSource *source = (NexCacheSource *)data;
    g_return_if_fail(source);
    source->poll_fd.events |= G_IO_IN;
    g_main_context_wakeup(g_source_get_context((GSource *)data));
}

static void
nexcache_source_del_read(gpointer data) {
    NexCacheSource *source = (NexCacheSource *)data;
    g_return_if_fail(source);
    source->poll_fd.events &= ~G_IO_IN;
    g_main_context_wakeup(g_source_get_context((GSource *)data));
}

static void
nexcache_source_add_write(gpointer data) {
    NexCacheSource *source = (NexCacheSource *)data;
    g_return_if_fail(source);
    source->poll_fd.events |= G_IO_OUT;
    g_main_context_wakeup(g_source_get_context((GSource *)data));
}

static void
nexcache_source_del_write(gpointer data) {
    NexCacheSource *source = (NexCacheSource *)data;
    g_return_if_fail(source);
    source->poll_fd.events &= ~G_IO_OUT;
    g_main_context_wakeup(g_source_get_context((GSource *)data));
}

static void
nexcache_source_cleanup(gpointer data) {
    NexCacheSource *source = (NexCacheSource *)data;

    g_return_if_fail(source);

    nexcache_source_del_read(source);
    nexcache_source_del_write(source);
    /*
     * It is not our responsibility to remove ourself from the
     * current main loop. However, we will remove the GPollFD.
     */
    if (source->poll_fd.fd >= 0) {
        g_source_remove_poll((GSource *)data, &source->poll_fd);
        source->poll_fd.fd = -1;
    }
}

static gboolean
nexcache_source_prepare(GSource *source,
                      gint *timeout_) {
    NexCacheSource *nexcache = (NexCacheSource *)source;
    *timeout_ = -1;
    return !!(nexcache->poll_fd.events & nexcache->poll_fd.revents);
}

static gboolean
nexcache_source_check(GSource *source) {
    NexCacheSource *nexcache = (NexCacheSource *)source;
    return !!(nexcache->poll_fd.events & nexcache->poll_fd.revents);
}

static gboolean
nexcache_source_dispatch(GSource *source,
                       GSourceFunc callback,
                       gpointer user_data) {
    NexCacheSource *nexcache = (NexCacheSource *)source;

    if ((nexcache->poll_fd.revents & G_IO_OUT)) {
        nexcacheAsyncHandleWrite(nexcache->ac);
        nexcache->poll_fd.revents &= ~G_IO_OUT;
    }

    if ((nexcache->poll_fd.revents & G_IO_IN)) {
        nexcacheAsyncHandleRead(nexcache->ac);
        nexcache->poll_fd.revents &= ~G_IO_IN;
    }

    if (callback) {
        return callback(user_data);
    }

    return TRUE;
}

static void
nexcache_source_finalize(GSource *source) {
    NexCacheSource *nexcache = (NexCacheSource *)source;

    if (nexcache->poll_fd.fd >= 0) {
        g_source_remove_poll(source, &nexcache->poll_fd);
        nexcache->poll_fd.fd = -1;
    }
}

static GSource *
nexcache_source_new(nexcacheAsyncContext *ac) {
    static GSourceFuncs source_funcs = {
        .prepare = nexcache_source_prepare,
        .check = nexcache_source_check,
        .dispatch = nexcache_source_dispatch,
        .finalize = nexcache_source_finalize,
    };
    nexcacheContext *c = &ac->c;
    NexCacheSource *source;

    g_return_val_if_fail(ac != NULL, NULL);

    source = (NexCacheSource *)g_source_new(&source_funcs, sizeof *source);
    if (source == NULL)
        return NULL;

    source->ac = ac;
    source->poll_fd.fd = c->fd;
    source->poll_fd.events = 0;
    source->poll_fd.revents = 0;
    g_source_add_poll((GSource *)source, &source->poll_fd);

    ac->ev.addRead = nexcache_source_add_read;
    ac->ev.delRead = nexcache_source_del_read;
    ac->ev.addWrite = nexcache_source_add_write;
    ac->ev.delWrite = nexcache_source_del_write;
    ac->ev.cleanup = nexcache_source_cleanup;
    ac->ev.data = source;

    return (GSource *)source;
}

/* Internal adapter function with correct function signature. */
static int nexcacheGlibAttachAdapter(nexcacheAsyncContext *ac, void *context) {
    if (g_source_attach(nexcache_source_new(ac), (GMainContext *)context) > 0) {
        return NEXCACHE_OK;
    }
    return NEXCACHE_ERR;
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseGlib(nexcacheClusterOptions *options,
                                       GMainContext *context) {
    if (options == NULL) { // A NULL context is accepted.
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheGlibAttachAdapter;
    options->attach_data = context;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_GLIB_H */
