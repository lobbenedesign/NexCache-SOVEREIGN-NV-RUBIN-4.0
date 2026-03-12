
#include "nexcachemodule.h"

static void timer_callback(NexCacheModuleCtx *ctx, void *data)
{
    NexCacheModuleString *keyname = data;
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx, "INCR", "s", keyname);
    if (reply != NULL)
        NexCacheModule_FreeCallReply(reply);
    NexCacheModule_FreeString(ctx, keyname);
}

int test_createtimer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long period;
    if (NexCacheModule_StringToLongLong(argv[1], &period) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx, "Invalid time specified.");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString *keyname = argv[2];
    NexCacheModule_RetainString(ctx, keyname);

    NexCacheModuleTimerID id = NexCacheModule_CreateTimer(ctx, period, timer_callback, keyname);
    NexCacheModule_ReplyWithLongLong(ctx, id);

    return NEXCACHEMODULE_OK;
}

int test_gettimer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long id;
    if (NexCacheModule_StringToLongLong(argv[1], &id) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx, "Invalid id specified.");
        return NEXCACHEMODULE_OK;
    }

    uint64_t remaining;
    NexCacheModuleString *keyname;
    if (NexCacheModule_GetTimerInfo(ctx, id, &remaining, (void **)&keyname) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithNull(ctx);
    } else {
        NexCacheModule_ReplyWithArray(ctx, 2);
        NexCacheModule_ReplyWithString(ctx, keyname);
        NexCacheModule_ReplyWithLongLong(ctx, remaining);
    }

    return NEXCACHEMODULE_OK;
}

int test_stoptimer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long id;
    if (NexCacheModule_StringToLongLong(argv[1], &id) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx, "Invalid id specified.");
        return NEXCACHEMODULE_OK;
    }

    int ret = 0;
    NexCacheModuleString *keyname;
    if (NexCacheModule_StopTimer(ctx, id, (void **) &keyname) == NEXCACHEMODULE_OK) {
        NexCacheModule_FreeString(ctx, keyname);
        ret = 1;
    }

    NexCacheModule_ReplyWithLongLong(ctx, ret);
    return NEXCACHEMODULE_OK;
}


int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"timer",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.createtimer", test_createtimer,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.gettimer", test_gettimer,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.stoptimer", test_stoptimer,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
