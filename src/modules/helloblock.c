/* Helloblock module -- An example of blocking command implementation
 * with threads.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2016, Redis Ltd.
 * All rights reserved.
 *
 * NexCachetribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * NexCachetributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * NexCachetributions in binary form must reproduce the above copyright
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

#include "../nexcachemodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* Reply callback for blocking command HELLO.BLOCK */
int HelloBlock_Reply(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    int *myint = NexCacheModule_GetBlockedClientPrivateData(ctx);
    return NexCacheModule_ReplyWithLongLong(ctx, *myint);
}

/* Timeout callback for blocking command HELLO.BLOCK */
int HelloBlock_Timeout(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

/* Private data freeing callback for HELLO.BLOCK command. */
void HelloBlock_FreeData(NexCacheModuleCtx *ctx, void *privdata) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModule_Free(privdata);
}

/* The thread entry point that actually executes the blocking part
 * of the command HELLO.BLOCK. */
void *HelloBlock_ThreadMain(void *arg) {
    void **targ = arg;
    NexCacheModuleBlockedClient *bc = targ[0];
    long long delay = (unsigned long)targ[1];
    NexCacheModule_Free(targ);

    sleep(delay);
    int *r = NexCacheModule_Alloc(sizeof(int));
    *r = rand();
    NexCacheModule_UnblockClient(bc, r);
    return NULL;
}

/* An example blocked client disconnection callback.
 *
 * Note that in the case of the HELLO.BLOCK command, the blocked client is now
 * owned by the thread calling sleep(). In this specific case, there is not
 * much we can do, however normally we could instead implement a way to
 * signal the thread that the client disconnected, and sleep the specified
 * amount of seconds with a while loop calling sleep(1), so that once we
 * detect the client disconnection, we can terminate the thread ASAP. */
void HelloBlock_Disconnected(NexCacheModuleCtx *ctx, NexCacheModuleBlockedClient *bc) {
    NexCacheModule_Log(ctx, "warning", "Blocked client %p disconnected!", (void *)bc);

    /* Here you should cleanup your state / threads, and if possible
     * call NexCacheModule_UnblockClient(), or notify the thread that will
     * call the function ASAP. */
}

/* HELLO.BLOCK <delay> <timeout> -- Block for <count> seconds, then reply with
 * a random number. Timeout is the command timeout, so that you can test
 * what happens when the delay is greater than the timeout. */
int HelloBlock_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    long long delay;
    long long timeout;

    if (NexCacheModule_StringToLongLong(argv[1], &delay) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");
    }

    if (NexCacheModule_StringToLongLong(argv[2], &timeout) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");
    }

    pthread_t tid;
    NexCacheModuleBlockedClient *bc =
        NexCacheModule_BlockClient(ctx, HelloBlock_Reply, HelloBlock_Timeout, HelloBlock_FreeData, timeout);

    /* Here we set a disconnection handler, however since this module will
     * block in sleep() in a thread, there is not much we can do in the
     * callback, so this is just to show you the API. */
    NexCacheModule_SetDisconnectCallback(bc, HelloBlock_Disconnected);

    /* Now that we setup a blocking client, we need to pass the control
     * to the thread. However we need to pass arguments to the thread:
     * the delay and a reference to the blocked client handle. */
    void **targ = NexCacheModule_Alloc(sizeof(void *) * 2);
    targ[0] = bc;
    targ[1] = (void *)(unsigned long)delay;

    if (pthread_create(&tid, NULL, HelloBlock_ThreadMain, targ) != 0) {
        NexCacheModule_AbortBlock(bc);
        return NexCacheModule_ReplyWithError(ctx, "-ERR Can't start thread");
    }
    return NEXCACHEMODULE_OK;
}

/* The thread entry point that actually executes the blocking part
 * of the command HELLO.KEYS.
 *
 * Note: this implementation is very simple on purpose, so no duplicated
 * keys (returned by SCAN) are filtered. However adding such a functionality
 * would be trivial just using any data structure implementing a dictionary
 * in order to filter the duplicated items. */
void *HelloKeys_ThreadMain(void *arg) {
    NexCacheModuleBlockedClient *bc = arg;
    NexCacheModuleCtx *ctx = NexCacheModule_GetThreadSafeContext(bc);
    long long cursor = 0;
    size_t replylen = 0;

    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    do {
        NexCacheModule_ThreadSafeContextLock(ctx);
        NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, "SCAN", "l", (long long)cursor);
        NexCacheModule_ThreadSafeContextUnlock(ctx);

        NexCacheModuleCallReply *cr_cursor = NexCacheModule_CallReplyArrayElement(reply, 0);
        NexCacheModuleCallReply *cr_keys = NexCacheModule_CallReplyArrayElement(reply, 1);

        NexCacheModuleString *s = NexCacheModule_CreateStringFromCallReply(cr_cursor);
        NexCacheModule_StringToLongLong(s, &cursor);
        NexCacheModule_FreeString(ctx, s);

        size_t items = NexCacheModule_CallReplyLength(cr_keys);
        for (size_t j = 0; j < items; j++) {
            NexCacheModuleCallReply *ele = NexCacheModule_CallReplyArrayElement(cr_keys, j);
            NexCacheModule_ReplyWithCallReply(ctx, ele);
            replylen++;
        }
        NexCacheModule_FreeCallReply(reply);
    } while (cursor != 0);
    NexCacheModule_ReplySetArrayLength(ctx, replylen);

    NexCacheModule_FreeThreadSafeContext(ctx);
    NexCacheModule_UnblockClient(bc, NULL);
    return NULL;
}

/* HELLO.KEYS -- Return all the keys in the current database without blocking
 * the server. The keys do not represent a point-in-time state so only the keys
 * that were in the database from the start to the end are guaranteed to be
 * there. */
int HelloKeys_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    pthread_t tid;

    /* Note that when blocking the client we do not set any callback: no
     * timeout is possible since we passed '0', nor we need a reply callback
     * because we'll use the thread safe context to accumulate a reply. */
    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);

    /* Now that we setup a blocking client, we need to pass the control
     * to the thread. However we need to pass arguments to the thread:
     * the reference to the blocked client handle. */
    if (pthread_create(&tid, NULL, HelloKeys_ThreadMain, bc) != 0) {
        NexCacheModule_AbortBlock(bc);
        return NexCacheModule_ReplyWithError(ctx, "-ERR Can't start thread");
    }
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "helloblock", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.block", HelloBlock_NexCacheCommand, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "hello.keys", HelloKeys_NexCacheCommand, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
