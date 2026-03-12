/*
 * Copyright (c) NexCache Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* ==========================================================================
 * trace_server.h - support lttng tracing for server events.
 * --------------------------------------------------------------------------
 * Copyright (C) 2025  zhenwei pi <zhenwei.pi@linux.dev>
 * Copyright (C) 2025  zhiqiang li <lizhiqiang.sf@bytedance.com>
 *
 * This work is licensed under BSD 3-Clause, License 1 of the COPYING file in
 * the top-level directory.
 * ==========================================================================
 */

#ifdef USE_LTTNG

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER nexcache_server

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "./trace_server.h"

#if !defined(__NEXCACHE_TRACE_SERVER_H__) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define __NEXCACHE_TRACE_SERVER_H__

#include <lttng/tracepoint.h>

LTTNG_UST_TRACEPOINT_EVENT_CLASS(
    /* Tracepoint class provider name */
    nexcache_server,

    /* Tracepoint class name */
    nexcache_server_class,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    ),

    /* List of fields of eventual event (output) */
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint64_t, duration, duration)
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, command_unblocking,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, while_blocked_cron,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, eventloop,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, eventloop_cron,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, module_acquire_gil,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, command,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(
    /* Name of the tracepoint class provider */
    nexcache_server, nexcache_server_class, nexcache_server, fast_command,

    /* List of tracepoint arguments (input) */
    LTTNG_UST_TP_ARGS(
      uint64_t, duration
    )
)

#define nexcache_server_trace(...) lttng_ust_tracepoint(__VA_ARGS__)

#endif /* __NEXCACHE_TRACE_SERVER_H__ */

#include <lttng/tracepoint-event.h>

#else /* USE_LTTNG */

#ifndef __NEXCACHE_TRACE_SERVER_H__
#define __NEXCACHE_TRACE_SERVER_H__

/* avoid compiler warning on empty source file */
static inline void __nexcache_server_trace(void) {
}

#define nexcache_server_trace(...) \
    do {                     \
    } while (0)

#endif /* __NEXCACHE_TRACE_SERVER_H__ */

#endif /* USE_LTTNG */
