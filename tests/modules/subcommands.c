#include "nexcachemodule.h"

#define UNUSED(V) ((void) V)

int cmd_set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int cmd_get(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);

    if (argc > 4) /* For testing */
        return NexCacheModule_WrongArity(ctx);

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int cmd_get_fullname(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    const char *command_name = NexCacheModule_GetCurrentCommandName(ctx);
    NexCacheModule_ReplyWithSimpleString(ctx, command_name);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "subcommands", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Module command names cannot contain special characters. */
    NexCacheModule_Assert(NexCacheModule_CreateCommand(ctx,"subcommands.char\r",NULL,"",0,0,0) == NEXCACHEMODULE_ERR);
    NexCacheModule_Assert(NexCacheModule_CreateCommand(ctx,"subcommands.char\n",NULL,"",0,0,0) == NEXCACHEMODULE_ERR);
    NexCacheModule_Assert(NexCacheModule_CreateCommand(ctx,"subcommands.char ",NULL,"",0,0,0) == NEXCACHEMODULE_ERR);

    if (NexCacheModule_CreateCommand(ctx,"subcommands.bitarray",NULL,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    NexCacheModuleCommand *parent = NexCacheModule_GetCommand(ctx,"subcommands.bitarray");

    if (NexCacheModule_CreateSubcommand(parent,"set",cmd_set,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Module subcommand names cannot contain special characters. */
    NexCacheModule_Assert(NexCacheModule_CreateSubcommand(parent,"char|",cmd_set,"",0,0,0) == NEXCACHEMODULE_ERR);
    NexCacheModule_Assert(NexCacheModule_CreateSubcommand(parent,"char@",cmd_set,"",0,0,0) == NEXCACHEMODULE_ERR);
    NexCacheModule_Assert(NexCacheModule_CreateSubcommand(parent,"char=",cmd_set,"",0,0,0) == NEXCACHEMODULE_ERR);

    NexCacheModuleCommand *subcmd = NexCacheModule_GetCommand(ctx,"subcommands.bitarray|set");
    NexCacheModuleCommandInfo cmd_set_info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(subcmd, &cmd_set_info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateSubcommand(parent,"get",cmd_get,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    subcmd = NexCacheModule_GetCommand(ctx,"subcommands.bitarray|get");
    NexCacheModuleCommandInfo cmd_get_info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(subcmd, &cmd_get_info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Get the name of the command currently running. */
    if (NexCacheModule_CreateCommand(ctx,"subcommands.parent_get_fullname",cmd_get_fullname,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Get the name of the subcommand currently running. */
    if (NexCacheModule_CreateCommand(ctx,"subcommands.sub",NULL,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *fullname_parent = NexCacheModule_GetCommand(ctx,"subcommands.sub");
    if (NexCacheModule_CreateSubcommand(fullname_parent,"get_fullname",cmd_get_fullname,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Sanity */

    /* Trying to create the same subcommand fails */
    NexCacheModule_Assert(NexCacheModule_CreateSubcommand(parent,"get",NULL,"",0,0,0) == NEXCACHEMODULE_ERR);

    /* Trying to create a sub-subcommand fails */
    NexCacheModule_Assert(NexCacheModule_CreateSubcommand(subcmd,"get",NULL,"",0,0,0) == NEXCACHEMODULE_ERR);

    return NEXCACHEMODULE_OK;
}
