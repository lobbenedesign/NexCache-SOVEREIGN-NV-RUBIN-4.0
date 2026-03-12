
#include "nexcachemodule.h"
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#define UNUSED(V) ((void) V)

/* A sample movable keys command that returns a list of all
 * arguments that follow a KEY argument, i.e.
 */
int getkeys_command(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    int i;
    int count = 0;

    /* Handle getkeys-api introspection */
    if (NexCacheModule_IsKeysPositionRequest(ctx)) {
        for (i = 0; i < argc; i++) {
            size_t len;
            const char *str = NexCacheModule_StringPtrLen(argv[i], &len);

            if (len == 3 && !strncasecmp(str, "key", 3) && i + 1 < argc)
                NexCacheModule_KeyAtPos(ctx, i + 1);
        }

        return NEXCACHEMODULE_OK;
    }

    /* Handle real command invocation */
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    for (i = 0; i < argc; i++) {
        size_t len;
        const char *str = NexCacheModule_StringPtrLen(argv[i], &len);

        if (len == 3 && !strncasecmp(str, "key", 3) && i + 1 < argc) {
            NexCacheModule_ReplyWithString(ctx, argv[i+1]);
            count++;
        }
    }
    NexCacheModule_ReplySetArrayLength(ctx, count);

    return NEXCACHEMODULE_OK;
}

int getkeys_command_with_flags(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    int i;
    int count = 0;

    /* Handle getkeys-api introspection */
    if (NexCacheModule_IsKeysPositionRequest(ctx)) {
        for (i = 0; i < argc; i++) {
            size_t len;
            const char *str = NexCacheModule_StringPtrLen(argv[i], &len);

            if (len == 3 && !strncasecmp(str, "key", 3) && i + 1 < argc)
                NexCacheModule_KeyAtPosWithFlags(ctx, i + 1, NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS);
        }

        return NEXCACHEMODULE_OK;
    }

    /* Handle real command invocation */
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    for (i = 0; i < argc; i++) {
        size_t len;
        const char *str = NexCacheModule_StringPtrLen(argv[i], &len);

        if (len == 3 && !strncasecmp(str, "key", 3) && i + 1 < argc) {
            NexCacheModule_ReplyWithString(ctx, argv[i+1]);
            count++;
        }
    }
    NexCacheModule_ReplySetArrayLength(ctx, count);

    return NEXCACHEMODULE_OK;
}

int getkeys_fixed(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    int i;

    NexCacheModule_ReplyWithArray(ctx, argc - 1);
    for (i = 1; i < argc; i++) {
        NexCacheModule_ReplyWithString(ctx, argv[i]);
    }
    return NEXCACHEMODULE_OK;
}

/* Introspect a command using RM_GetCommandKeys() and returns the list
 * of keys. Essentially this is COMMAND GETKEYS implemented in a module.
 * INTROSPECT <with-flags> <cmd> <args>
 */
int getkeys_introspect(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    long long with_flags = 0;

    if (argc < 4) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    if (NexCacheModule_StringToLongLong(argv[1],&with_flags) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid integer");

    int num_keys, *keyflags = NULL;
    int *keyidx = NexCacheModule_GetCommandKeysWithFlags(ctx, &argv[2], argc - 2, &num_keys, with_flags ? &keyflags : NULL);

    if (!keyidx) {
        if (!errno)
            NexCacheModule_ReplyWithEmptyArray(ctx);
        else {
            char err[100];
            switch (errno) {
                case ENOENT:
                    NexCacheModule_ReplyWithError(ctx, "ERR ENOENT");
                    break;
                case EINVAL:
                    NexCacheModule_ReplyWithError(ctx, "ERR EINVAL");
                    break;
                default:
                    snprintf(err, sizeof(err) - 1, "ERR errno=%d", errno);
                    NexCacheModule_ReplyWithError(ctx, err);
                    break;
            }
        }
    } else {
        int i;

        NexCacheModule_ReplyWithArray(ctx, num_keys);
        for (i = 0; i < num_keys; i++) {
            if (!with_flags) {
                NexCacheModule_ReplyWithString(ctx, argv[2 + keyidx[i]]);
                continue;
            }
            NexCacheModule_ReplyWithArray(ctx, 2);
            NexCacheModule_ReplyWithString(ctx, argv[2 + keyidx[i]]);
            char* sflags = "";
            if (keyflags[i] & NEXCACHEMODULE_CMD_KEY_RO)
                sflags = "RO";
            else if (keyflags[i] & NEXCACHEMODULE_CMD_KEY_RW)
                sflags = "RW";
            else if (keyflags[i] & NEXCACHEMODULE_CMD_KEY_OW)
                sflags = "OW";
            else if (keyflags[i] & NEXCACHEMODULE_CMD_KEY_RM)
                sflags = "RM";
            NexCacheModule_ReplyWithCString(ctx, sflags);
        }

        NexCacheModule_Free(keyidx);
        NexCacheModule_Free(keyflags);
    }

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    if (NexCacheModule_Init(ctx,"getkeys",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"getkeys.command", getkeys_command,"getkeys-api",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"getkeys.command_with_flags", getkeys_command_with_flags,"getkeys-api",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"getkeys.fixed", getkeys_fixed,"",2,4,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"getkeys.introspect", getkeys_introspect,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
