/* This module is used to test the propagation (replication + AOF) of
 * commands, via the NexCacheModule_Replicate() interface, in asynchronous
 * contexts, such as callbacks not implementing commands, and thread safe
 * contexts.
 *
 * We create a timer callback and a threads using a thread safe context.
 * Using both we try to propagate counters increments, and later we check
 * if the replica contains the changes as expected.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2019, NexCache Contributors.
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

#include "nexcachemodule.h"
#include <pthread.h>
#include <errno.h>

#define UNUSED(V) ((void) V)

NexCacheModuleCtx *detached_ctx = NULL;

static int KeySpace_NotificationGeneric(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NEXCACHEMODULE_NOT_USED(key);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "c!", "notifications");
    NexCacheModule_FreeCallReply(rep);

    return NEXCACHEMODULE_OK;
}

/* Timer callback. */
void timerHandler(NexCacheModuleCtx *ctx, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(data);

    static int times = 0;

    NexCacheModule_Replicate(ctx,"INCR","c","timer");
    times++;

    if (times < 3)
        NexCacheModule_CreateTimer(ctx,100,timerHandler,NULL);
    else
        times = 0;
}

int propagateTestTimerCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleTimerID timer_id =
        NexCacheModule_CreateTimer(ctx,100,timerHandler,NULL);
    NEXCACHEMODULE_NOT_USED(timer_id);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

/* Timer callback. */
void timerNestedHandler(NexCacheModuleCtx *ctx, void *data) {
    int repl = (long long)data;

    /* The goal is the trigger a module command that calls RM_Replicate
     * in order to test MULTI/EXEC structure */
    NexCacheModule_Replicate(ctx,"INCRBY","cc","timer-nested-start","1");
    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx,"propagate-test.nested", repl? "!" : "");
    NexCacheModule_FreeCallReply(reply);
    reply = NexCacheModule_Call(ctx, "INCR", repl? "c!" : "c", "timer-nested-middle");
    NexCacheModule_FreeCallReply(reply);
    NexCacheModule_Replicate(ctx,"INCRBY","cc","timer-nested-end","1");
}

int propagateTestTimerNestedCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleTimerID timer_id =
        NexCacheModule_CreateTimer(ctx,100,timerNestedHandler,(void*)0);
    NEXCACHEMODULE_NOT_USED(timer_id);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

int propagateTestTimerNestedReplCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleTimerID timer_id =
        NexCacheModule_CreateTimer(ctx,100,timerNestedHandler,(void*)1);
    NEXCACHEMODULE_NOT_USED(timer_id);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

void timerHandlerMaxmemory(NexCacheModuleCtx *ctx, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(data);

    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx,"SETEX","ccc!","timer-maxmemory-volatile-start","100","1");
    NexCacheModule_FreeCallReply(reply);
    reply = NexCacheModule_Call(ctx, "CONFIG", "ccc!", "SET", "maxmemory", "1");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_Replicate(ctx, "INCR", "c", "timer-maxmemory-middle");

    reply = NexCacheModule_Call(ctx,"SETEX","ccc!","timer-maxmemory-volatile-end","100","1");
    NexCacheModule_FreeCallReply(reply);
}

int propagateTestTimerMaxmemoryCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleTimerID timer_id =
        NexCacheModule_CreateTimer(ctx,100,timerHandlerMaxmemory,(void*)1);
    NEXCACHEMODULE_NOT_USED(timer_id);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

void timerHandlerEval(NexCacheModuleCtx *ctx, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(data);

    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx,"INCRBY","cc!","timer-eval-start","1");
    NexCacheModule_FreeCallReply(reply);
    reply = NexCacheModule_Call(ctx, "EVAL", "cccc!", "nexcache.call('set',KEYS[1],ARGV[1])", "1", "foo", "bar");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_Replicate(ctx, "INCR", "c", "timer-eval-middle");

    reply = NexCacheModule_Call(ctx,"INCRBY","cc!","timer-eval-end","1");
    NexCacheModule_FreeCallReply(reply);
}

int propagateTestTimerEvalCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleTimerID timer_id =
        NexCacheModule_CreateTimer(ctx,100,timerHandlerEval,(void*)1);
    NEXCACHEMODULE_NOT_USED(timer_id);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

/* The thread entry point. */
void *threadMain(void *arg) {
    NEXCACHEMODULE_NOT_USED(arg);
    NexCacheModuleCtx *ctx = NexCacheModule_GetThreadSafeContext(NULL);
    NexCacheModule_SelectDb(ctx,9); /* Tests ran in database number 9. */
    for (int i = 0; i < 3; i++) {
        NexCacheModule_ThreadSafeContextLock(ctx);
        NexCacheModule_Replicate(ctx,"INCR","c","a-from-thread");
        NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx,"INCR","c!","thread-call");
        NexCacheModule_FreeCallReply(reply);
        NexCacheModule_Replicate(ctx,"INCR","c","b-from-thread");
        NexCacheModule_ThreadSafeContextUnlock(ctx);
    }
    NexCacheModule_FreeThreadSafeContext(ctx);
    return NULL;
}

int propagateTestThreadCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    pthread_t tid;
    if (pthread_create(&tid,NULL,threadMain,NULL) != 0)
        return NexCacheModule_ReplyWithError(ctx,"-ERR Can't start thread");
    NEXCACHEMODULE_NOT_USED(tid);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

/* The thread entry point. */
void *threadDetachedMain(void *arg) {
    NEXCACHEMODULE_NOT_USED(arg);
    NexCacheModule_SelectDb(detached_ctx,9); /* Tests ran in database number 9. */

    NexCacheModule_ThreadSafeContextLock(detached_ctx);
    NexCacheModule_Replicate(detached_ctx,"INCR","c","thread-detached-before");
    NexCacheModuleCallReply *reply = NexCacheModule_Call(detached_ctx,"INCR","c!","thread-detached-1");
    NexCacheModule_FreeCallReply(reply);
    reply = NexCacheModule_Call(detached_ctx,"INCR","c!","thread-detached-2");
    NexCacheModule_FreeCallReply(reply);
    NexCacheModule_Replicate(detached_ctx,"INCR","c","thread-detached-after");
    NexCacheModule_ThreadSafeContextUnlock(detached_ctx);

    return NULL;
}

int propagateTestDetachedThreadCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    pthread_t tid;
    if (pthread_create(&tid,NULL,threadDetachedMain,NULL) != 0)
        return NexCacheModule_ReplyWithError(ctx,"-ERR Can't start thread");
    NEXCACHEMODULE_NOT_USED(tid);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

int propagateTestSimpleCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    /* Replicate two commands to test MULTI/EXEC wrapping. */
    NexCacheModule_Replicate(ctx, "INCR", "c", "counter-1");
    NexCacheModule_Replicate(ctx, "INCR", "c", "counter-2");
    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

int propagateTestMixedCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleCallReply *reply;

    /* This test mixes multiple propagation systems. */
    reply = NexCacheModule_Call(ctx, "INCR", "c!", "using-call");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_Replicate(ctx, "INCR", "c", "counter-1");
    NexCacheModule_Replicate(ctx, "INCR", "c", "counter-2");

    reply = NexCacheModule_Call(ctx, "INCR", "c!", "after-call");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

int propagateTestNestedCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleCallReply *reply;

    /* This test mixes multiple propagation systems. */
    reply = NexCacheModule_Call(ctx, "INCR", "c!", "using-call");
    NexCacheModule_FreeCallReply(reply);

    reply = NexCacheModule_Call(ctx,"propagate-test.simple", "!");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_Replicate(ctx,"INCR","c","counter-3");
    NexCacheModule_Replicate(ctx,"INCR","c","counter-4");

    reply = NexCacheModule_Call(ctx, "INCR", "c!", "after-call");
    NexCacheModule_FreeCallReply(reply);

    reply = NexCacheModule_Call(ctx, "INCR", "c!", "before-call-2");
    NexCacheModule_FreeCallReply(reply);

    reply = NexCacheModule_Call(ctx, "keyspace.incr_case1", "c!", "asdf"); /* Propagates INCR */
    NexCacheModule_FreeCallReply(reply);

    reply = NexCacheModule_Call(ctx, "keyspace.del_key_copy", "c!", "asdf"); /* Propagates DEL */
    NexCacheModule_FreeCallReply(reply);

    reply = NexCacheModule_Call(ctx, "INCR", "c!", "after-call-2");
    NexCacheModule_FreeCallReply(reply);

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;
}

/* Counter to track "propagate-test.incr" commands which were obeyed (due to being replicated or processed from AOF). */
static long long obeyed_cmds = 0;

/* Handles the "propagate-test.obeyed" command to return the `obeyed_cmds` count. */
int propagateTestObeyed(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_ReplyWithLongLong(ctx, obeyed_cmds);
    return NEXCACHEMODULE_OK;
}

int propagateTestIncr(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleCallReply *reply;

    /* Track the number of commands which are "obeyed". */
    if (NexCacheModule_MustObeyClient(ctx)) {
        obeyed_cmds += 1;
    }
    /* This test propagates the module command, not the INCR it executes. */
    reply = NexCacheModule_Call(ctx, "INCR", "s", argv[1]);
    NexCacheModule_ReplyWithCallReply(ctx,reply);
    NexCacheModule_FreeCallReply(reply);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"propagate-test",1,NEXCACHEMODULE_APIVER_1)
            == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    detached_ctx = NexCacheModule_GetDetachedThreadSafeContext(ctx);

    /* This option tests skip command validation for NexCacheModule_Replicate */
    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_SKIP_COMMAND_VALIDATION);

    if (NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_ALL, KeySpace_NotificationGeneric) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.timer",
                propagateTestTimerCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.timer-nested",
                propagateTestTimerNestedCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.timer-nested-repl",
                propagateTestTimerNestedReplCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.timer-maxmemory",
                propagateTestTimerMaxmemoryCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.timer-eval",
                propagateTestTimerEvalCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.thread",
                propagateTestThreadCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.detached-thread",
                propagateTestDetachedThreadCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.simple",
                propagateTestSimpleCommand,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.mixed",
                propagateTestMixedCommand,
                "write",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.nested",
                propagateTestNestedCommand,
                "write",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.incr",
                propagateTestIncr,
                "write",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"propagate-test.obeyed",
                propagateTestObeyed,
                "",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    UNUSED(ctx);

    if (detached_ctx)
        NexCacheModule_FreeThreadSafeContext(detached_ctx);

    return NEXCACHEMODULE_OK;
}
