#include "nexcachemodule.h"
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

/* A sample with declarable channels, that are used to validate against ACLs */
int getChannels_subscribe(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if ((argc - 1) % 3 != 0) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    char *err = NULL;
    
    /* getchannels.command [[subscribe|unsubscribe|publish] [pattern|literal] <channel> ...]
     * This command marks the given channel is accessed based on the
     * provided modifiers. */
    for (int i = 1; i < argc; i += 3) {
        const char *operation = NexCacheModule_StringPtrLen(argv[i], NULL);
        const char *type = NexCacheModule_StringPtrLen(argv[i+1], NULL);
        int flags = 0;

        if (!strcasecmp(operation, "subscribe")) {
            flags |= NEXCACHEMODULE_CMD_CHANNEL_SUBSCRIBE;
        } else if (!strcasecmp(operation, "unsubscribe")) {
            flags |= NEXCACHEMODULE_CMD_CHANNEL_UNSUBSCRIBE;
        } else if (!strcasecmp(operation, "publish")) {
            flags |= NEXCACHEMODULE_CMD_CHANNEL_PUBLISH;
        } else {
            err = "Invalid channel operation";
            break;
        }

        if (!strcasecmp(type, "literal")) {
            /* No op */
        } else if (!strcasecmp(type, "pattern")) {
            flags |= NEXCACHEMODULE_CMD_CHANNEL_PATTERN;
        } else {
            err = "Invalid channel type";
            break;
        }
        if (NexCacheModule_IsChannelsPositionRequest(ctx)) {
            NexCacheModule_ChannelAtPosWithFlags(ctx, i+2, flags);
        }
    }

    if (!NexCacheModule_IsChannelsPositionRequest(ctx)) {
        if (err) {
            NexCacheModule_ReplyWithError(ctx, err);
        } else {
            /* Normal implementation would go here, but for tests just return okay */
            NexCacheModule_ReplyWithSimpleString(ctx, "OK");
        }
    }

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "getchannels", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "getchannels.command", getChannels_subscribe, "getchannels-api", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
