#include "nexcachemodule.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define UNUSED(V) ((void) V)

int cmd_publish_classic_multi(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc < 3)
        return NexCacheModule_WrongArity(ctx);
    NexCacheModule_ReplyWithArray(ctx, argc-2);
    for (int i = 2; i < argc; i++) {
        int receivers = NexCacheModule_PublishMessage(ctx, argv[1], argv[i]);
        NexCacheModule_ReplyWithLongLong(ctx, receivers);
    }
    return NEXCACHEMODULE_OK;
}

int cmd_publish_classic(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);
    
    int receivers = NexCacheModule_PublishMessage(ctx, argv[1], argv[2]);
    NexCacheModule_ReplyWithLongLong(ctx, receivers);
    return NEXCACHEMODULE_OK;
}

int cmd_publish_shard(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);
    
    int receivers = NexCacheModule_PublishMessageShard(ctx, argv[1], argv[2]);
    NexCacheModule_ReplyWithLongLong(ctx, receivers);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    
    if (NexCacheModule_Init(ctx,"publish",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"publish.classic",cmd_publish_classic,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"publish.classic_multi",cmd_publish_classic_multi,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"publish.shard",cmd_publish_shard,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
        
    return NEXCACHEMODULE_OK;
}
