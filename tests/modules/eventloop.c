/* This module contains four tests :
 * 1- test.sanity    : Basic tests for argument validation mostly.
 * 2- test.sendbytes : Creates a pipe and registers its fds to the event loop,
 *                     one end of the pipe for read events and the other end for
 *                     the write events. On writable event, data is written. On
 *                     readable event data is read. Repeated until all data is
 *                     received.
 * 3- test.iteration : A test for BEFORE_SLEEP and AFTER_SLEEP callbacks.
 *                     Counters are incremented each time these events are
 *                     fired. They should be equal and increment monotonically.
 * 4- test.oneshot   : Test for oneshot API
 */

#include "nexcachemodule.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>

int fds[2];
long long buf_size;
char *src;
long long src_offset;
char *dst;
long long dst_offset;

NexCacheModuleBlockedClient *bc;
NexCacheModuleCtx *reply_ctx;

void onReadable(int fd, void *user_data, int mask) {
    NEXCACHEMODULE_NOT_USED(mask);

    NexCacheModule_Assert(strcmp(user_data, "userdataread") == 0);

    while (1) {
        int rd = read(fd, dst + dst_offset, buf_size - dst_offset);
        if (rd <= 0)
            return;
        dst_offset += rd;

        /* Received all bytes */
        if (dst_offset == buf_size) {
            if (memcmp(src, dst, buf_size) == 0)
                NexCacheModule_ReplyWithSimpleString(reply_ctx, "OK");
            else
                NexCacheModule_ReplyWithError(reply_ctx, "ERR bytes mismatch");

            NexCacheModule_EventLoopDel(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE);
            NexCacheModule_EventLoopDel(fds[1], NEXCACHEMODULE_EVENTLOOP_WRITABLE);
            NexCacheModule_Free(src);
            NexCacheModule_Free(dst);
            close(fds[0]);
            close(fds[1]);

            NexCacheModule_FreeThreadSafeContext(reply_ctx);
            NexCacheModule_UnblockClient(bc, NULL);
            return;
        }
    };
}

void onWritable(int fd, void *user_data, int mask) {
    NEXCACHEMODULE_NOT_USED(user_data);
    NEXCACHEMODULE_NOT_USED(mask);

    NexCacheModule_Assert(strcmp(user_data, "userdatawrite") == 0);

    while (1) {
        /* Check if we sent all data */
        if (src_offset >= buf_size)
            return;
        int written = write(fd, src + src_offset, buf_size - src_offset);
        if (written <= 0) {
            return;
        }

        src_offset += written;
    };
}

/* Create a pipe(), register pipe fds to the event loop and send/receive data
 * using them. */
int sendbytes(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    if (NexCacheModule_StringToLongLong(argv[1], &buf_size) != NEXCACHEMODULE_OK ||
        buf_size == 0) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }

    bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
    reply_ctx = NexCacheModule_GetThreadSafeContext(bc);

    /* Allocate source buffer and write some random data */
    src = NexCacheModule_Calloc(1,buf_size);
    src_offset = 0;
    memset(src, rand() % 0xFF, buf_size);
    memcpy(src, "randomtestdata", strlen("randomtestdata"));

    dst = NexCacheModule_Calloc(1,buf_size);
    dst_offset = 0;

    /* Create a pipe and register it to the event loop. */
    if (pipe(fds) < 0) return NEXCACHEMODULE_ERR;
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) return NEXCACHEMODULE_ERR;
    if (fcntl(fds[1], F_SETFL, O_NONBLOCK) < 0) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_EventLoopAdd(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE,
        onReadable, "userdataread") != NEXCACHEMODULE_OK) return NEXCACHEMODULE_ERR;
    if (NexCacheModule_EventLoopAdd(fds[1], NEXCACHEMODULE_EVENTLOOP_WRITABLE,
        onWritable, "userdatawrite") != NEXCACHEMODULE_OK) return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}

int sanity(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (pipe(fds) < 0) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_EventLoopAdd(fds[0], 9999999, onReadable, NULL)
        == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, "ERR non-existing event type should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(-1, NEXCACHEMODULE_EVENTLOOP_READABLE, onReadable, NULL)
        == NEXCACHEMODULE_OK || errno != ERANGE) {
        NexCacheModule_ReplyWithError(ctx, "ERR out of range fd should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(99999999, NEXCACHEMODULE_EVENTLOOP_READABLE, onReadable, NULL)
        == NEXCACHEMODULE_OK || errno != ERANGE) {
        NexCacheModule_ReplyWithError(ctx, "ERR out of range fd should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE, NULL, NULL)
        == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, "ERR null callback should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(fds[0], 9999999, onReadable, NULL)
        == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, "ERR non-existing event type should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopDel(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE)
        != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, "ERR del on non-registered fd should not fail");
        goto out;
    }
    if (NexCacheModule_EventLoopDel(fds[0], 9999999) == NEXCACHEMODULE_OK ||
        errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, "ERR non-existing event type should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopDel(-1, NEXCACHEMODULE_EVENTLOOP_READABLE)
        == NEXCACHEMODULE_OK || errno != ERANGE) {
        NexCacheModule_ReplyWithError(ctx, "ERR out of range fd should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopDel(99999999, NEXCACHEMODULE_EVENTLOOP_READABLE)
        == NEXCACHEMODULE_OK || errno != ERANGE) {
        NexCacheModule_ReplyWithError(ctx, "ERR out of range fd should fail");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE, onReadable, NULL)
        != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, "ERR Add failed");
        goto out;
    }
    if (NexCacheModule_EventLoopAdd(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE, onReadable, NULL)
        != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, "ERR Adding same fd twice failed");
        goto out;
    }
    if (NexCacheModule_EventLoopDel(fds[0], NEXCACHEMODULE_EVENTLOOP_READABLE)
        != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, "ERR Del failed");
        goto out;
    }
    if (NexCacheModule_EventLoopAddOneShot(NULL, NULL) == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, "ERR null callback should fail");
        goto out;
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
out:
    close(fds[0]);
    close(fds[1]);
    return NEXCACHEMODULE_OK;
}

static long long beforeSleepCount;
static long long afterSleepCount;

int iteration(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    /* On each event loop iteration, eventloopCallback() is called. We increment
     * beforeSleepCount and afterSleepCount, so these two should be equal.
     * We reply with iteration count, caller can test if iteration count
     * increments monotonically */
    NexCacheModule_Assert(beforeSleepCount == afterSleepCount);
    NexCacheModule_ReplyWithLongLong(ctx, beforeSleepCount);
    return NEXCACHEMODULE_OK;
}

void oneshotCallback(void* arg)
{
    NexCacheModule_Assert(strcmp(arg, "userdata") == 0);
    NexCacheModule_ReplyWithSimpleString(reply_ctx, "OK");
    NexCacheModule_FreeThreadSafeContext(reply_ctx);
    NexCacheModule_UnblockClient(bc, NULL);
}

int oneshot(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
    reply_ctx = NexCacheModule_GetThreadSafeContext(bc);

    if (NexCacheModule_EventLoopAddOneShot(oneshotCallback, "userdata") != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "ERR oneshot failed");
        NexCacheModule_FreeThreadSafeContext(reply_ctx);
        NexCacheModule_UnblockClient(bc, NULL);
    }
    return NEXCACHEMODULE_OK;
}

void eventloopCallback(struct NexCacheModuleCtx *ctx, NexCacheModuleEvent eid, uint64_t subevent, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(eid);
    NEXCACHEMODULE_NOT_USED(subevent);
    NEXCACHEMODULE_NOT_USED(data);

    NexCacheModule_Assert(eid.id == NEXCACHEMODULE_EVENT_EVENTLOOP);
    if (subevent == NEXCACHEMODULE_SUBEVENT_EVENTLOOP_BEFORE_SLEEP)
        beforeSleepCount++;
    else if (subevent == NEXCACHEMODULE_SUBEVENT_EVENTLOOP_AFTER_SLEEP)
        afterSleepCount++;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"eventloop",1,NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Test basics. */
    if (NexCacheModule_CreateCommand(ctx, "test.sanity", sanity, "", 0, 0, 0)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Register a command to create a pipe() and send data through it by using
     * event loop API. */
    if (NexCacheModule_CreateCommand(ctx, "test.sendbytes", sendbytes, "", 0, 0, 0)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Register a command to return event loop iteration count. */
    if (NexCacheModule_CreateCommand(ctx, "test.iteration", iteration, "", 0, 0, 0)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.oneshot", oneshot, "", 0, 0, 0)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_EventLoop,
        eventloopCallback) != NEXCACHEMODULE_OK) return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
