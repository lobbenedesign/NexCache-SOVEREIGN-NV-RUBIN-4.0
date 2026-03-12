/* define macros for having usleep */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <unistd.h>

#include "nexcachemodule.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <strings.h>

#define UNUSED(V) ((void) V)

/* used to test processing events during slow bg operation */
static volatile int g_slow_bg_operation = 0;
static volatile int g_is_in_slow_bg_operation = 0;

void *sub_worker(void *arg) {
    // Get module context
    NexCacheModuleCtx *ctx = (NexCacheModuleCtx *)arg;

    // Try acquiring GIL
    int res = NexCacheModule_ThreadSafeContextTryLock(ctx);

    // GIL is already taken by the calling thread expecting to fail.
    assert(res != NEXCACHEMODULE_OK);

    return NULL;
}

void *worker(void *arg) {
    // Retrieve blocked client
    NexCacheModuleBlockedClient *bc = (NexCacheModuleBlockedClient *)arg;

    // Get module context
    NexCacheModuleCtx *ctx = NexCacheModule_GetThreadSafeContext(bc);

    // Acquire GIL
    NexCacheModule_ThreadSafeContextLock(ctx);

    // Create another thread which will try to acquire the GIL
    pthread_t tid;
    int res = pthread_create(&tid, NULL, sub_worker, ctx);
    assert(res == 0);

    // Wait for thread
    pthread_join(tid, NULL);

    // Release GIL
    NexCacheModule_ThreadSafeContextUnlock(ctx);

    // Reply to client
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

    // Unblock client
    NexCacheModule_UnblockClient(bc, NULL);

    // Free the module context
    NexCacheModule_FreeThreadSafeContext(ctx);

    return NULL;
}

int acquire_gil(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    UNUSED(argv);
    UNUSED(argc);

    int flags = NexCacheModule_GetContextFlags(ctx);
    int allFlags = NexCacheModule_GetContextFlagsAll();
    if ((allFlags & NEXCACHEMODULE_CTX_FLAGS_MULTI) &&
        (flags & NEXCACHEMODULE_CTX_FLAGS_MULTI)) {
        NexCacheModule_ReplyWithSimpleString(ctx, "Blocked client is not supported inside multi");
        return NEXCACHEMODULE_OK;
    }

    if ((allFlags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING) &&
        (flags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING)) {
        NexCacheModule_ReplyWithSimpleString(ctx, "Blocked client is not allowed");
        return NEXCACHEMODULE_OK;
    }

    /* This command handler tries to acquire the GIL twice
     * once in the worker thread using "NexCacheModule_ThreadSafeContextLock"
     * second in the sub-worker thread
     * using "NexCacheModule_ThreadSafeContextTryLock"
     * as the GIL is already locked. */
    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);

    pthread_t tid;
    int res = pthread_create(&tid, NULL, worker, bc);
    assert(res == 0);

    return NEXCACHEMODULE_OK;
}

typedef struct {
    NexCacheModuleString **argv;
    int argc;
    NexCacheModuleBlockedClient *bc;
} bg_call_data;

void *bg_call_worker(void *arg) {
    bg_call_data *bg = arg;
    NexCacheModuleBlockedClient *bc = bg->bc;

    // Get module context
    NexCacheModuleCtx *ctx = NexCacheModule_GetThreadSafeContext(bg->bc);

    // Acquire GIL
    NexCacheModule_ThreadSafeContextLock(ctx);

    // Test slow operation yielding
    if (g_slow_bg_operation) {
        g_is_in_slow_bg_operation = 1;
        while (g_slow_bg_operation) {
            NexCacheModule_Yield(ctx, NEXCACHEMODULE_YIELD_FLAG_CLIENTS, "Slow module operation");
            usleep(1000);
        }
        g_is_in_slow_bg_operation = 0;
    }

    // Call the command
    const char *module_cmd = NexCacheModule_StringPtrLen(bg->argv[0], NULL);
    int cmd_pos = 1;
    NexCacheModuleString *format_nexcache_str = NexCacheModule_CreateString(NULL, "v", 1);
    if (!strcasecmp(module_cmd, "do_bg_rm_call_format")) {
        cmd_pos = 2;
        size_t format_len;
        const char *format = NexCacheModule_StringPtrLen(bg->argv[1], &format_len);
        NexCacheModule_StringAppendBuffer(NULL, format_nexcache_str, format, format_len);
        NexCacheModule_StringAppendBuffer(NULL, format_nexcache_str, "E", 1);
    }
    const char *format = NexCacheModule_StringPtrLen(format_nexcache_str, NULL);
    const char *cmd = NexCacheModule_StringPtrLen(bg->argv[cmd_pos], NULL);
    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, cmd, format, bg->argv + cmd_pos + 1, (size_t)bg->argc - cmd_pos - 1);
    NexCacheModule_FreeString(NULL, format_nexcache_str);

    /* Free the arguments within GIL to prevent simultaneous freeing in main thread. */
    for (int i=0; i<bg->argc; i++)
        NexCacheModule_FreeString(ctx, bg->argv[i]);
    NexCacheModule_Free(bg->argv);
    NexCacheModule_Free(bg);

    // Release GIL
    NexCacheModule_ThreadSafeContextUnlock(ctx);

    // Reply to client
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    // Unblock client
    NexCacheModule_UnblockClient(bc, NULL);

    // Free the module context
    NexCacheModule_FreeThreadSafeContext(ctx);

    return NULL;
}

int do_bg_rm_call(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    UNUSED(argv);
    UNUSED(argc);

    /* Make sure we're not trying to block a client when we shouldn't */
    int flags = NexCacheModule_GetContextFlags(ctx);
    int allFlags = NexCacheModule_GetContextFlagsAll();
    if ((allFlags & NEXCACHEMODULE_CTX_FLAGS_MULTI) &&
        (flags & NEXCACHEMODULE_CTX_FLAGS_MULTI)) {
        NexCacheModule_ReplyWithSimpleString(ctx, "Blocked client is not supported inside multi");
        return NEXCACHEMODULE_OK;
    }
    if ((allFlags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING) &&
        (flags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING)) {
        NexCacheModule_ReplyWithSimpleString(ctx, "Blocked client is not allowed");
        return NEXCACHEMODULE_OK;
    }

    /* Make a copy of the arguments and pass them to the thread. */
    bg_call_data *bg = NexCacheModule_Alloc(sizeof(bg_call_data));
    bg->argv = NexCacheModule_Alloc(sizeof(NexCacheModuleString*)*argc);
    bg->argc = argc;
    for (int i=0; i<argc; i++)
        bg->argv[i] = NexCacheModule_HoldString(ctx, argv[i]);

    /* Block the client */
    bg->bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);

    /* Start a thread to handle the request */
    pthread_t tid;
    int res = pthread_create(&tid, NULL, bg_call_worker, bg);
    assert(res == 0);

    return NEXCACHEMODULE_OK;
}

int do_rm_call(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "Ev", argv + 2, (size_t)argc - 2);
    if(!rep){
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    }else{
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

static void rm_call_async_send_reply(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply) {
    NexCacheModule_ReplyWithCallReply(ctx, reply);
    NexCacheModule_FreeCallReply(reply);
}

/* Called when the command that was blocked on 'RM_Call' gets unblocked
 * and send the reply to the blocked client. */
static void rm_call_async_on_unblocked(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, void *private_data) {
    UNUSED(ctx);
    NexCacheModuleBlockedClient *bc = private_data;
    NexCacheModuleCtx *bctx = NexCacheModule_GetThreadSafeContext(bc);
    rm_call_async_send_reply(bctx, reply);
    NexCacheModule_FreeThreadSafeContext(bctx);
    NexCacheModule_UnblockClient(bc, NexCacheModule_BlockClientGetPrivateData(bc));
}

int do_rm_call_async_fire_and_forget(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }
    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "!KEv", argv + 2, (size_t)argc - 2);

    if(NexCacheModule_CallReplyType(rep) != NEXCACHEMODULE_REPLY_PROMISE) {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "Blocked");
    }
    NexCacheModule_FreeCallReply(rep);

    return NEXCACHEMODULE_OK;
}

static void do_rm_call_async_free_pd(NexCacheModuleCtx * ctx, void *pd) {
    UNUSED(ctx);
    NexCacheModule_FreeCallReply(pd);
}

static void do_rm_call_async_disconnect(NexCacheModuleCtx *ctx, struct NexCacheModuleBlockedClient *bc) {
    UNUSED(ctx);
    NexCacheModuleCallReply* rep = NexCacheModule_BlockClientGetPrivateData(bc);
    NexCacheModule_CallReplyPromiseAbort(rep, NULL);
    NexCacheModule_FreeCallReply(rep);
    NexCacheModule_AbortBlock(bc);
}

/*
 * Callback for do_rm_call_async / do_rm_call_async_script_mode
 * Gets the command to invoke as the first argument to the command and runs it,
 * passing the rest of the arguments to the command invocation.
 * If the command got blocked, blocks the client and unblock it when the command gets unblocked,
 * this allows check the K (allow blocking) argument to RM_Call.
 */
int do_rm_call_async(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    size_t format_len = 0;
    char format[6] = {0};

    if (!(NexCacheModule_GetContextFlags(ctx) & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING)) {
        /* We are allowed to block the client so we can allow RM_Call to also block us */
        format[format_len++] = 'K';
    }

    const char* invoked_cmd = NexCacheModule_StringPtrLen(argv[0], NULL);
    if (strcasecmp(invoked_cmd, "do_rm_call_async_script_mode") == 0) {
        format[format_len++] = 'S';
    }

    format[format_len++] = 'E';
    format[format_len++] = 'v';
    if (strcasecmp(invoked_cmd, "do_rm_call_async_no_replicate") != 0) {
        /* Notice, without the '!' flag we will have inconsistency between master and replica.
         * This is used only to check '!' flag correctness on blocked commands. */
        format[format_len++] = '!';
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, format, argv + 2, (size_t)argc - 2);

    if(NexCacheModule_CallReplyType(rep) != NEXCACHEMODULE_REPLY_PROMISE) {
        rm_call_async_send_reply(ctx, rep);
    } else {
        NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, do_rm_call_async_free_pd, 0);
        NexCacheModule_SetDisconnectCallback(bc, do_rm_call_async_disconnect);
        NexCacheModule_BlockClientSetPrivateData(bc, rep);
        NexCacheModule_CallReplyPromiseSetUnblockHandler(rep, rm_call_async_on_unblocked, bc);
    }

    return NEXCACHEMODULE_OK;
}

typedef struct ThreadedAsyncRMCallCtx{
    NexCacheModuleBlockedClient *bc;
    NexCacheModuleCallReply *reply;
} ThreadedAsyncRMCallCtx;

void *send_async_reply(void *arg) {
    ThreadedAsyncRMCallCtx *ta_rm_call_ctx = arg;
    rm_call_async_on_unblocked(NULL, ta_rm_call_ctx->reply, ta_rm_call_ctx->bc);
    NexCacheModule_Free(ta_rm_call_ctx);
    return NULL;
}

/* Called when the command that was blocked on 'RM_Call' gets unblocked
 * and schedule a thread to send the reply to the blocked client. */
static void rm_call_async_reply_on_thread(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, void *private_data) {
    UNUSED(ctx);
    ThreadedAsyncRMCallCtx *ta_rm_call_ctx = NexCacheModule_Alloc(sizeof(*ta_rm_call_ctx));
    ta_rm_call_ctx->bc = private_data;
    ta_rm_call_ctx->reply = reply;
    pthread_t tid;
    int res = pthread_create(&tid, NULL, send_async_reply, ta_rm_call_ctx);
    assert(res == 0);
}

/*
 * Callback for do_rm_call_async_on_thread.
 * Gets the command to invoke as the first argument to the command and runs it,
 * passing the rest of the arguments to the command invocation.
 * If the command got blocked, blocks the client and unblock on a background thread.
 * this allows check the K (allow blocking) argument to RM_Call, and make sure that the reply
 * that passes to unblock handler is owned by the handler and are not attached to any
 * context that might be freed after the callback ends.
 */
int do_rm_call_async_on_thread(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "KEv", argv + 2, (size_t)argc - 2);

    if(NexCacheModule_CallReplyType(rep) != NEXCACHEMODULE_REPLY_PROMISE) {
        rm_call_async_send_reply(ctx, rep);
    } else {
        NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
        NexCacheModule_CallReplyPromiseSetUnblockHandler(rep, rm_call_async_reply_on_thread, bc);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

/* Private data for wait_and_do_rm_call_async that holds information about:
 * 1. the block client, to unblock when done.
 * 2. the arguments, contains the command to run using RM_Call */
typedef struct WaitAndDoRMCallCtx {
    NexCacheModuleBlockedClient *bc;
    NexCacheModuleString **argv;
    int argc;
} WaitAndDoRMCallCtx;

/*
 * This callback will be called when the 'wait' command invoke on 'wait_and_do_rm_call_async' will finish.
 * This callback will continue the execution flow just like 'do_rm_call_async' command.
 */
static void wait_and_do_rm_call_async_on_unblocked(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, void *private_data) {
    WaitAndDoRMCallCtx *wctx = private_data;
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_INTEGER) {
        goto done;
    }

    if (NexCacheModule_CallReplyInteger(reply) != 1) {
        goto done;
    }

    NexCacheModule_FreeCallReply(reply);
    reply = NULL;

    const char* cmd = NexCacheModule_StringPtrLen(wctx->argv[0], NULL);
    reply = NexCacheModule_Call(ctx, cmd, "!EKv", wctx->argv + 1, (size_t)wctx->argc - 1);

done:
    if(NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_PROMISE) {
        NexCacheModuleCtx *bctx = NexCacheModule_GetThreadSafeContext(wctx->bc);
        rm_call_async_send_reply(bctx, reply);
        NexCacheModule_FreeThreadSafeContext(bctx);
        NexCacheModule_UnblockClient(wctx->bc, NULL);
    } else {
        NexCacheModule_CallReplyPromiseSetUnblockHandler(reply, rm_call_async_on_unblocked, wctx->bc);
        NexCacheModule_FreeCallReply(reply);
    }
    for (int i = 0 ; i < wctx->argc ; ++i) {
        NexCacheModule_FreeString(NULL, wctx->argv[i]);
    }
    NexCacheModule_Free(wctx->argv);
    NexCacheModule_Free(wctx);
}

/*
 * Callback for wait_and_do_rm_call
 * Gets the command to invoke as the first argument, runs 'wait'
 * command (using the K flag to RM_Call). Once the wait finished, runs the
 * command that was given (just like 'do_rm_call_async').
 */
int wait_and_do_rm_call_async(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    int flags = NexCacheModule_GetContextFlags(ctx);
    if (flags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING) {
        return NexCacheModule_ReplyWithError(ctx, "Err can not run wait, blocking is not allowed.");
    }

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "wait", "!EKcc", "1", "0");
    if(NexCacheModule_CallReplyType(rep) != NEXCACHEMODULE_REPLY_PROMISE) {
        rm_call_async_send_reply(ctx, rep);
    } else {
        NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
        WaitAndDoRMCallCtx *wctx = NexCacheModule_Alloc(sizeof(*wctx));
        *wctx = (WaitAndDoRMCallCtx){
                .bc = bc,
                .argv = NexCacheModule_Alloc((argc - 1) * sizeof(NexCacheModuleString*)),
                .argc = argc - 1,
        };

        for (int i = 1 ; i < argc ; ++i) {
            wctx->argv[i - 1] = NexCacheModule_HoldString(NULL, argv[i]);
        }
        NexCacheModule_CallReplyPromiseSetUnblockHandler(rep, wait_and_do_rm_call_async_on_unblocked, wctx);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

static void blpop_and_set_multiple_keys_on_unblocked(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, void *private_data) {
    /* ignore the reply */
    NexCacheModule_FreeCallReply(reply);
    WaitAndDoRMCallCtx *wctx = private_data;
    for (int i = 0 ; i < wctx->argc ; i += 2) {
        NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "set", "!ss", wctx->argv[i], wctx->argv[i + 1]);
        NexCacheModule_FreeCallReply(rep);
    }

    NexCacheModuleCtx *bctx = NexCacheModule_GetThreadSafeContext(wctx->bc);
    NexCacheModule_ReplyWithSimpleString(bctx, "OK");
    NexCacheModule_FreeThreadSafeContext(bctx);
    NexCacheModule_UnblockClient(wctx->bc, NULL);

    for (int i = 0 ; i < wctx->argc ; ++i) {
        NexCacheModule_FreeString(NULL, wctx->argv[i]);
    }
    NexCacheModule_Free(wctx->argv);
    NexCacheModule_Free(wctx);

}

/*
 * Performs a blpop command on a given list and when unblocked set multiple string keys.
 * This command allows checking that the unblock callback is performed as a unit
 * and its effect are replicated to the replica and AOF wrapped with multi exec.
 */
int blpop_and_set_multiple_keys(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    if(argc < 2 || argc % 2 != 0){
        return NexCacheModule_WrongArity(ctx);
    }

    int flags = NexCacheModule_GetContextFlags(ctx);
    if (flags & NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING) {
        return NexCacheModule_ReplyWithError(ctx, "Err can not run wait, blocking is not allowed.");
    }

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "blpop", "!EKsc", argv[1], "0");
    if(NexCacheModule_CallReplyType(rep) != NEXCACHEMODULE_REPLY_PROMISE) {
        rm_call_async_send_reply(ctx, rep);
    } else {
        NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
        WaitAndDoRMCallCtx *wctx = NexCacheModule_Alloc(sizeof(*wctx));
        *wctx = (WaitAndDoRMCallCtx){
                .bc = bc,
                .argv = NexCacheModule_Alloc((argc - 2) * sizeof(NexCacheModuleString*)),
                .argc = argc - 2,
        };

        for (int i = 0 ; i < argc - 2 ; ++i) {
            wctx->argv[i] = NexCacheModule_HoldString(NULL, argv[i + 2]);
        }
        NexCacheModule_CallReplyPromiseSetUnblockHandler(rep, blpop_and_set_multiple_keys_on_unblocked, wctx);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

/* simulate a blocked client replying to a thread safe context without creating a thread */
int do_fake_bg_true(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
    NexCacheModuleCtx *bctx = NexCacheModule_GetThreadSafeContext(bc);

    NexCacheModule_ReplyWithBool(bctx, 1);

    NexCacheModule_FreeThreadSafeContext(bctx);
    NexCacheModule_UnblockClient(bc, NULL);

    return NEXCACHEMODULE_OK;
}


/* this flag is used to work with busy commands, that might take a while
 * and ability to stop the busy work with a different command*/
static volatile int abort_flag = 0;

int slow_fg_command(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    long long block_time = 0;
    if (NexCacheModule_StringToLongLong(argv[1], &block_time) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }

    uint64_t start_time = NexCacheModule_MonotonicMicroseconds();
    /* when not blocking indefinitely, we don't process client commands in this test. */
    int yield_flags = block_time? NEXCACHEMODULE_YIELD_FLAG_NONE: NEXCACHEMODULE_YIELD_FLAG_CLIENTS;
    while (!abort_flag) {
        NexCacheModule_Yield(ctx, yield_flags, "Slow module operation");
        usleep(1000);
        if (block_time && NexCacheModule_MonotonicMicroseconds() - start_time > (uint64_t)block_time)
            break;
    }

    abort_flag = 0;
    NexCacheModule_ReplyWithLongLong(ctx, 1);
    return NEXCACHEMODULE_OK;
}

int stop_slow_fg_command(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    abort_flag = 1;
    NexCacheModule_ReplyWithLongLong(ctx, 1);
    return NEXCACHEMODULE_OK;
}

/* used to enable or disable slow operation in do_bg_rm_call */
static int set_slow_bg_operation(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    long long ll;
    if (NexCacheModule_StringToLongLong(argv[1], &ll) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }
    g_slow_bg_operation = ll;
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* used to test if we reached the slow operation in do_bg_rm_call */
static int is_in_slow_bg_operation(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    if (argc != 1) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithLongLong(ctx, g_is_in_slow_bg_operation);
    return NEXCACHEMODULE_OK;
}

static void timer_callback(NexCacheModuleCtx *ctx, void *data)
{
    UNUSED(ctx);

    NexCacheModuleBlockedClient *bc = data;

    // Get module context
    NexCacheModuleCtx *reply_ctx = NexCacheModule_GetThreadSafeContext(bc);

    // Reply to client
    NexCacheModule_ReplyWithSimpleString(reply_ctx, "OK");

    // Unblock client
    NexCacheModule_UnblockClient(bc, NULL);

    // Free the module context
    NexCacheModule_FreeThreadSafeContext(reply_ctx);
}

/* unblock_by_timer <period_ms> <timeout_ms>
 * period_ms is the period of the timer.
 * timeout_ms is the blocking timeout. */
int unblock_by_timer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);

    long long period;
    long long timeout;
    if (NexCacheModule_StringToLongLong(argv[1],&period) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid period");
    if (NexCacheModule_StringToLongLong(argv[2],&timeout) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid timeout");
    }

    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, timeout);
    NexCacheModule_CreateTimer(ctx, period, timer_callback, bc);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "blockedclient", 1, NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "acquire_gil", acquire_gil, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call", do_rm_call,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call_async", do_rm_call_async,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call_async_on_thread", do_rm_call_async_on_thread,
                                      "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call_async_script_mode", do_rm_call_async,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call_async_no_replicate", do_rm_call_async,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_rm_call_fire_and_forget", do_rm_call_async_fire_and_forget,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "wait_and_do_rm_call", wait_and_do_rm_call_async,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blpop_and_set_multiple_keys", blpop_and_set_multiple_keys,
                                      "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_bg_rm_call", do_bg_rm_call, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_bg_rm_call_format", do_bg_rm_call, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "do_fake_bg_true", do_fake_bg_true, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "slow_fg_command", slow_fg_command,"", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "stop_slow_fg_command", stop_slow_fg_command,"allow-busy", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "set_slow_bg_operation", set_slow_bg_operation, "allow-busy", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "is_in_slow_bg_operation", is_in_slow_bg_operation, "allow-busy", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "unblock_by_timer", unblock_by_timer, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
