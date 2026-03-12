#include "nexcachemodule.h"
#include <string.h>

#define UNUSED(x) (void)(x)

void cluster_timer_handler(NexCacheModuleCtx *ctx, void *data) {
    NEXCACHEMODULE_NOT_USED(data);

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "CLUSTER", "c", "SLOTS");

    if (rep) {
        if (NexCacheModule_CallReplyType(rep) == NEXCACHEMODULE_REPLY_ARRAY) {
            NexCacheModule_Log(ctx, "notice", "Timer: CLUSTER SLOTS success");
        } else {
            NexCacheModule_Log(ctx, "notice",
                             "Timer: CLUSTER SLOTS unexpected reply type %d",
                             NexCacheModule_CallReplyType(rep));
        }
        NexCacheModule_FreeCallReply(rep);
    } else {
        NexCacheModule_Log(ctx, "warning", "Timer: CLUSTER SLOTS failed");
    }
}

int test_start_cluster_timer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_CreateTimer(ctx, 1, cluster_timer_handler, NULL);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}


int test_cluster_slots(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);

    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "CLUSTER", "c", "SLOTS");
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "ERR NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

int test_cluster_shards(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);

    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "CLUSTER", "c", "SHARDS");
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "ERR NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

#define MSGTYPE_DING 1
#define MSGTYPE_DONG 2

/* test.pingall */
int PingallCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_SendClusterMessage(ctx, NULL, MSGTYPE_DING, "Hey", 3);
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

void DingReceiver(NexCacheModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {
    NexCacheModule_Log(ctx, "notice", "DING (type %d) RECEIVED from %.*s: '%.*s'", type, NEXCACHEMODULE_NODE_ID_LEN, sender_id, (int)len, payload);
    /* Ensure sender_id is null-terminated for cross-version compatibility */
    char null_terminated_sender_id[NEXCACHEMODULE_NODE_ID_LEN + 1];
    memcpy(null_terminated_sender_id, sender_id, NEXCACHEMODULE_NODE_ID_LEN);
    null_terminated_sender_id[NEXCACHEMODULE_NODE_ID_LEN] = '\0';
    NexCacheModule_SendClusterMessage(ctx, null_terminated_sender_id, MSGTYPE_DONG, "Message Received!", 17);
}

void DongReceiver(NexCacheModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {
    NexCacheModule_Log(ctx, "notice", "DONG (type %d) RECEIVED from %.*s: '%.*s'", type, NEXCACHEMODULE_NODE_ID_LEN, sender_id, (int)len, payload);
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "cluster", 1, NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.pingall", PingallCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.cluster_slots", test_cluster_slots, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.cluster_shards", test_cluster_shards, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.start_cluster_timer", test_start_cluster_timer, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Register our handlers for different message types. */
    NexCacheModule_RegisterClusterMessageReceiver(ctx, MSGTYPE_DING, DingReceiver);
    NexCacheModule_RegisterClusterMessageReceiver(ctx, MSGTYPE_DONG, DongReceiver);

    return NEXCACHEMODULE_OK;
}
