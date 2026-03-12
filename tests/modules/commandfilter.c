#include "nexcachemodule.h"

#include <string.h>
#include <strings.h>

static NexCacheModuleString *log_key_name;

static const char log_command_name[] = "commandfilter.log";
static const char ping_command_name[] = "commandfilter.ping";
static const char retained_command_name[] = "commandfilter.retained";
static const char unregister_command_name[] = "commandfilter.unregister";
static const char unfiltered_clientid_name[] = "unfilter_clientid";
static int in_log_command = 0;

unsigned long long unfiltered_clientid = 0;

static NexCacheModuleCommandFilter *filter, *filter1;
static NexCacheModuleString *retained;

int CommandFilter_UnregisterCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    (void) argc;
    (void) argv;

    NexCacheModule_ReplyWithLongLong(ctx,
            NexCacheModule_UnregisterCommandFilter(ctx, filter));

    return NEXCACHEMODULE_OK;
}

int CommandFilter_PingCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    (void) argc;
    (void) argv;

    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, "ping", "c", "@log");
    if (reply) {
        NexCacheModule_ReplyWithCallReply(ctx, reply);
        NexCacheModule_FreeCallReply(reply);
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "Unknown command or invalid arguments");
    }

    return NEXCACHEMODULE_OK;
}

int CommandFilter_Retained(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    (void) argc;
    (void) argv;

    if (retained) {
        NexCacheModule_ReplyWithString(ctx, retained);
    } else {
        NexCacheModule_ReplyWithNull(ctx);
    }

    return NEXCACHEMODULE_OK;
}

int CommandFilter_LogCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NexCacheModuleString *s = NexCacheModule_CreateString(ctx, "", 0);

    int i;
    for (i = 1; i < argc; i++) {
        size_t arglen;
        const char *arg = NexCacheModule_StringPtrLen(argv[i], &arglen);

        if (i > 1) NexCacheModule_StringAppendBuffer(ctx, s, " ", 1);
        NexCacheModule_StringAppendBuffer(ctx, s, arg, arglen);
    }

    NexCacheModuleKey *log = NexCacheModule_OpenKey(ctx, log_key_name, NEXCACHEMODULE_WRITE|NEXCACHEMODULE_READ);
    NexCacheModule_ListPush(log, NEXCACHEMODULE_LIST_HEAD, s);
    NexCacheModule_CloseKey(log);
    NexCacheModule_FreeString(ctx, s);

    in_log_command = 1;

    size_t cmdlen;
    const char *cmdname = NexCacheModule_StringPtrLen(argv[1], &cmdlen);
    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, cmdname, "v", &argv[2], (size_t)argc - 2);
    if (reply) {
        NexCacheModule_ReplyWithCallReply(ctx, reply);
        NexCacheModule_FreeCallReply(reply);
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "Unknown command or invalid arguments");
    }

    in_log_command = 0;

    return NEXCACHEMODULE_OK;
}

int CommandFilter_UnfilteredClientId(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc < 2)
        return NexCacheModule_WrongArity(ctx);

    long long id;
    if (NexCacheModule_StringToLongLong(argv[1], &id) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "invalid client id");
        return NEXCACHEMODULE_OK;
    }
    if (id < 0) {
        NexCacheModule_ReplyWithError(ctx, "invalid client id");
        return NEXCACHEMODULE_OK;
    }

    unfiltered_clientid = id;
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* Filter to protect against Bug #11894 reappearing
 *
 * ensures that the filter is only run the first time through, and not on reprocessing
 */
void CommandFilter_BlmoveSwap(NexCacheModuleCommandFilterCtx *filter)
{
    if (NexCacheModule_CommandFilterArgsCount(filter) != 6)
        return;

    NexCacheModuleString *arg = NexCacheModule_CommandFilterArgGet(filter, 0);
    size_t arg_len;
    const char *arg_str = NexCacheModule_StringPtrLen(arg, &arg_len);

    if (arg_len != 6 || strncmp(arg_str, "blmove", 6))
        return;

    /*
     * Swapping directional args (right/left) from source and destination.
     * need to hold here, can't push into the ArgReplace func, as it will cause other to freed -> use after free
     */
    NexCacheModuleString *dir1 = NexCacheModule_HoldString(NULL, NexCacheModule_CommandFilterArgGet(filter, 3));
    NexCacheModuleString *dir2 = NexCacheModule_HoldString(NULL, NexCacheModule_CommandFilterArgGet(filter, 4));
    NexCacheModule_CommandFilterArgReplace(filter, 3, dir2);
    NexCacheModule_CommandFilterArgReplace(filter, 4, dir1);
}

void CommandFilter_CommandFilter(NexCacheModuleCommandFilterCtx *filter)
{
    unsigned long long id = NexCacheModule_CommandFilterGetClientId(filter);
    if (id == unfiltered_clientid) return;

    if (in_log_command) return;  /* don't process our own RM_Call() from CommandFilter_LogCommand() */

    /* Fun manipulations:
     * - Remove @delme
     * - Replace @replaceme
     * - Append @insertbefore or @insertafter
     * - Prefix with Log command if @log encountered
     */
    int log = 0;
    int pos = 0;
    while (pos < NexCacheModule_CommandFilterArgsCount(filter)) {
        const NexCacheModuleString *arg = NexCacheModule_CommandFilterArgGet(filter, pos);
        size_t arg_len;
        const char *arg_str = NexCacheModule_StringPtrLen(arg, &arg_len);

        if (arg_len == 6 && !memcmp(arg_str, "@delme", 6)) {
            NexCacheModule_CommandFilterArgDelete(filter, pos);
            continue;
        } 
        if (arg_len == 10 && !memcmp(arg_str, "@replaceme", 10)) {
            NexCacheModule_CommandFilterArgReplace(filter, pos,
                    NexCacheModule_CreateString(NULL, "--replaced--", 12));
        } else if (arg_len == 13 && !memcmp(arg_str, "@insertbefore", 13)) {
            NexCacheModule_CommandFilterArgInsert(filter, pos,
                    NexCacheModule_CreateString(NULL, "--inserted-before--", 19));
            pos++;
        } else if (arg_len == 12 && !memcmp(arg_str, "@insertafter", 12)) {
            NexCacheModule_CommandFilterArgInsert(filter, pos + 1,
                    NexCacheModule_CreateString(NULL, "--inserted-after--", 18));
            pos++;
        } else if (arg_len == 7 && !memcmp(arg_str, "@retain", 7)) {
            if (retained) NexCacheModule_FreeString(NULL, retained);
            retained = NexCacheModule_CommandFilterArgGet(filter, pos + 1);
            NexCacheModule_RetainString(NULL, retained);
            pos++;
        } else if (arg_len == 4 && !memcmp(arg_str, "@log", 4)) {
            log = 1;
        }
        pos++;
    }

    if (log) NexCacheModule_CommandFilterArgInsert(filter, 0,
            NexCacheModule_CreateString(NULL, log_command_name, sizeof(log_command_name)-1));
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (NexCacheModule_Init(ctx,"commandfilter",1,NEXCACHEMODULE_APIVER_1)
            == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (argc != 2 && argc != 3) {
        NexCacheModule_Log(ctx, "warning", "Log key name not specified");
        return NEXCACHEMODULE_ERR;
    }

    long long noself = 0;
    log_key_name = NexCacheModule_CreateStringFromString(ctx, argv[0]);
    NexCacheModule_StringToLongLong(argv[1], &noself);
    retained = NULL;

    if (NexCacheModule_CreateCommand(ctx,log_command_name,
                CommandFilter_LogCommand,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,ping_command_name,
                CommandFilter_PingCommand,"deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,retained_command_name,
                CommandFilter_Retained,"readonly",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,unregister_command_name,
                CommandFilter_UnregisterCommand,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, unfiltered_clientid_name,
                CommandFilter_UnfilteredClientId, "admin", 1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if ((filter = NexCacheModule_RegisterCommandFilter(ctx, CommandFilter_CommandFilter, 
                    noself ? NEXCACHEMODULE_CMDFILTER_NOSELF : 0))
            == NULL) return NEXCACHEMODULE_ERR;

    if ((filter1 = NexCacheModule_RegisterCommandFilter(ctx, CommandFilter_BlmoveSwap, 0)) == NULL)
        return NEXCACHEMODULE_ERR;

    if (argc == 3) {
        const char *ptr = NexCacheModule_StringPtrLen(argv[2], NULL);
        if (!strcasecmp(ptr, "noload")) {
            /* This is a hint that we return ERR at the last moment of OnLoad. */
            NexCacheModule_FreeString(ctx, log_key_name);
            if (retained) NexCacheModule_FreeString(NULL, retained);
            return NEXCACHEMODULE_ERR;
        }
    }

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NexCacheModule_FreeString(ctx, log_key_name);
    if (retained) NexCacheModule_FreeString(NULL, retained);

    return NEXCACHEMODULE_OK;
}
