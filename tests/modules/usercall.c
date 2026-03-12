#include "nexcachemodule.h"
#include <pthread.h>
#include <assert.h>

#define UNUSED(V) ((void) V)

NexCacheModuleUser *user = NULL;

int call_without_user(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 2) {
        return NexCacheModule_WrongArity(ctx);
    }

    const char *cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, cmd, "Ev", argv + 2, (size_t)argc - 2);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    return NEXCACHEMODULE_OK;
}

int call_with_user_flag(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    NexCacheModule_SetContextUser(ctx, user);

    /* Append Ev to the provided flags. */
    NexCacheModuleString *flags = NexCacheModule_CreateStringFromString(ctx, argv[1]);
    NexCacheModule_StringAppendBuffer(ctx, flags, "Ev", 2);

    const char* flg = NexCacheModule_StringPtrLen(flags, NULL);
    const char* cmd = NexCacheModule_StringPtrLen(argv[2], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, flg, argv + 3, (size_t)argc - 3);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    NexCacheModule_FreeString(ctx, flags);

    return NEXCACHEMODULE_OK;
}

int add_to_acl(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        return NexCacheModule_WrongArity(ctx);
    }

    size_t acl_len;
    const char *acl = NexCacheModule_StringPtrLen(argv[1], &acl_len);

    NexCacheModuleString *error;
    int ret = NexCacheModule_SetModuleUserACLString(ctx, user, acl, &error);
    if (ret) {
        size_t len;
        const char * e = NexCacheModule_StringPtrLen(error, &len);
        NexCacheModule_ReplyWithError(ctx, e);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

    return NEXCACHEMODULE_OK;
}

int get_acl(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);

    if (argc != 1) {
        return NexCacheModule_WrongArity(ctx);
    }

    NexCacheModule_Assert(user != NULL);

    NexCacheModuleString *acl = NexCacheModule_GetModuleUserACLString(user);

    NexCacheModule_ReplyWithString(ctx, acl);

    NexCacheModule_FreeString(NULL, acl);

    return NEXCACHEMODULE_OK;
}

int reset_user(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);

    if (argc != 1) {
        return NexCacheModule_WrongArity(ctx);
    }

    if (user != NULL) {
        NexCacheModule_FreeModuleUser(user);
    }

    user = NexCacheModule_CreateModuleUser("module_user");

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

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

    // Set user
    NexCacheModule_SetContextUser(ctx, user);

    // Call the command
    size_t format_len;
    NexCacheModuleString *format_nexcache_str = NexCacheModule_CreateString(NULL, "v", 1);
    const char *format = NexCacheModule_StringPtrLen(bg->argv[1], &format_len);
    NexCacheModule_StringAppendBuffer(NULL, format_nexcache_str, format, format_len);
    NexCacheModule_StringAppendBuffer(NULL, format_nexcache_str, "E", 1);
    format = NexCacheModule_StringPtrLen(format_nexcache_str, NULL);
    const char *cmd = NexCacheModule_StringPtrLen(bg->argv[2], NULL);
    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, cmd, format, bg->argv + 3, (size_t)bg->argc - 3);
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

int call_with_user_bg(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
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

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"usercall",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"usercall.call_without_user", call_without_user,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"usercall.call_with_user_flag", call_with_user_flag,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "usercall.call_with_user_bg", call_with_user_bg, "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "usercall.add_to_acl", add_to_acl, "write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"usercall.reset_user", reset_user,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"usercall.get_acl", get_acl,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NEXCACHEMODULE_NOT_USED(ctx);

    if (user != NULL) {
        NexCacheModule_FreeModuleUser(user);
        user = NULL;
    }

    return NEXCACHEMODULE_OK;
}
