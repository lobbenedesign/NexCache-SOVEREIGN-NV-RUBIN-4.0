/*
 * Copyright (c) 2015 Дмитрий Бахвалов (Dmitry Bakhvalov)
 *
 * Permission for license update:
 *   https://github.com/nexcache/hinexcache/issues/1271#issuecomment-2258225227
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

#ifndef NEXCACHE_ADAPTERS_MACOSX_H
#define NEXCACHE_ADAPTERS_MACOSX_H

#include "../async.h"
#include "../cluster.h"
#include "../nexcache.h"

#include <CoreFoundation/CoreFoundation.h>

typedef struct {
    nexcacheAsyncContext *context;
    CFSocketRef socketRef;
    CFRunLoopSourceRef sourceRef;
} NexCacheRunLoop;

static int freeNexCacheRunLoop(NexCacheRunLoop *nexcacheRunLoop) {
    if (nexcacheRunLoop != NULL) {
        if (nexcacheRunLoop->sourceRef != NULL) {
            CFRunLoopSourceInvalidate(nexcacheRunLoop->sourceRef);
            CFRelease(nexcacheRunLoop->sourceRef);
        }
        if (nexcacheRunLoop->socketRef != NULL) {
            CFSocketInvalidate(nexcacheRunLoop->socketRef);
            CFRelease(nexcacheRunLoop->socketRef);
        }
        vk_free(nexcacheRunLoop);
    }
    return NEXCACHE_ERR;
}

static void nexcacheMacOSAddRead(void *privdata) {
    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)privdata;
    CFSocketEnableCallBacks(nexcacheRunLoop->socketRef, kCFSocketReadCallBack);
}

static void nexcacheMacOSDelRead(void *privdata) {
    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)privdata;
    CFSocketDisableCallBacks(nexcacheRunLoop->socketRef, kCFSocketReadCallBack);
}

static void nexcacheMacOSAddWrite(void *privdata) {
    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)privdata;
    CFSocketEnableCallBacks(nexcacheRunLoop->socketRef, kCFSocketWriteCallBack);
}

static void nexcacheMacOSDelWrite(void *privdata) {
    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)privdata;
    CFSocketDisableCallBacks(nexcacheRunLoop->socketRef, kCFSocketWriteCallBack);
}

static void nexcacheMacOSCleanup(void *privdata) {
    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)privdata;
    freeNexCacheRunLoop(nexcacheRunLoop);
}

static void nexcacheMacOSAsyncCallback(CFSocketRef __unused s, CFSocketCallBackType callbackType, CFDataRef __unused address, const void __unused *data, void *info) {
    nexcacheAsyncContext *context = (nexcacheAsyncContext *)info;

    switch (callbackType) {
    case kCFSocketReadCallBack:
        nexcacheAsyncHandleRead(context);
        break;

    case kCFSocketWriteCallBack:
        nexcacheAsyncHandleWrite(context);
        break;

    default:
        break;
    }
}

static int nexcacheMacOSAttach(nexcacheAsyncContext *nexcacheAsyncCtx, CFRunLoopRef runLoop) {
    nexcacheContext *nexcacheCtx = &(nexcacheAsyncCtx->c);

    /* Nothing should be attached when something is already attached */
    if (nexcacheAsyncCtx->ev.data != NULL)
        return NEXCACHE_ERR;

    NexCacheRunLoop *nexcacheRunLoop = (NexCacheRunLoop *)vk_calloc(1, sizeof(NexCacheRunLoop));
    if (nexcacheRunLoop == NULL)
        return NEXCACHE_ERR;

    /* Setup nexcache stuff */
    nexcacheRunLoop->context = nexcacheAsyncCtx;

    nexcacheAsyncCtx->ev.addRead = nexcacheMacOSAddRead;
    nexcacheAsyncCtx->ev.delRead = nexcacheMacOSDelRead;
    nexcacheAsyncCtx->ev.addWrite = nexcacheMacOSAddWrite;
    nexcacheAsyncCtx->ev.delWrite = nexcacheMacOSDelWrite;
    nexcacheAsyncCtx->ev.cleanup = nexcacheMacOSCleanup;
    nexcacheAsyncCtx->ev.data = nexcacheRunLoop;

    /* Initialize and install read/write events */
    CFSocketContext socketCtx = {0, nexcacheAsyncCtx, NULL, NULL, NULL};

    nexcacheRunLoop->socketRef = CFSocketCreateWithNative(NULL, nexcacheCtx->fd,
                                                        kCFSocketReadCallBack | kCFSocketWriteCallBack,
                                                        nexcacheMacOSAsyncCallback,
                                                        &socketCtx);
    if (!nexcacheRunLoop->socketRef)
        return freeNexCacheRunLoop(nexcacheRunLoop);

    nexcacheRunLoop->sourceRef = CFSocketCreateRunLoopSource(NULL, nexcacheRunLoop->socketRef, 0);
    if (!nexcacheRunLoop->sourceRef)
        return freeNexCacheRunLoop(nexcacheRunLoop);

    CFRunLoopAddSource(runLoop, nexcacheRunLoop->sourceRef, kCFRunLoopDefaultMode);

    return NEXCACHE_OK;
}

/* Internal adapter function with correct function signature. */
static int nexcacheMacOSAttachAdapter(nexcacheAsyncContext *ac, void *loop) {
    return nexcacheMacOSAttach(ac, (CFRunLoopRef)loop);
}

NEXCACHE_UNUSED
static int nexcacheClusterOptionsUseMacOS(nexcacheClusterOptions *options,
                                        CFRunLoopRef loop) {
    if (options == NULL || loop == NULL) {
        return NEXCACHE_ERR;
    }

    options->attach_fn = nexcacheMacOSAttachAdapter;
    options->attach_data = loop;
    return NEXCACHE_OK;
}

#endif /* NEXCACHE_ADAPTERS_MACOSX_H */
