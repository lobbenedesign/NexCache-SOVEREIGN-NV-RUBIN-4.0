#include "nexcachemodule.h"

#define UNUSED(V) ((void) V)

int cmd_xadd(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "cmdintrospection", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"cmdintrospection.xadd",cmd_xadd,"write deny-oom random fast",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *xadd = NexCacheModule_GetCommand(ctx,"cmdintrospection.xadd");

    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .arity = -5,
        .summary = "Appends a new message to a stream. Creates the key if it doesn't exist.",
        .since = "5.0.0",
        .complexity = "O(1) when adding a new entry, O(N) when trimming where N being the number of entries evicted.",
        .tips = "nondeterministic_output",
        .history = (NexCacheModuleCommandHistoryEntry[]){
            /* NOTE: All versions specified should be the module's versions, not
             * the server's! We use server versions in this example for the purpose of
             * testing (comparing the output with the output of the vanilla
             * XADD). */
            {"6.2.0", "Added the `NOMKSTREAM` option, `MINID` trimming strategy and the `LIMIT` option."},
            {"7.0.0", "Added support for the `<ms>-*` explicit ID form."},
            {0}
        },
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .notes = "UPDATE instead of INSERT because of the optional trimming feature",
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {0}
        },
        .args = (NexCacheModuleCommandArg[]){
            {
                .name = "key",
                .type = NEXCACHEMODULE_ARG_TYPE_KEY,
                .key_spec_index = 0
            },
            {
                .name = "nomkstream",
                .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                .token = "NOMKSTREAM",
                .since = "6.2.0",
                .flags = NEXCACHEMODULE_CMD_ARG_OPTIONAL
            },
            {
                .name = "trim",
                .type = NEXCACHEMODULE_ARG_TYPE_BLOCK,
                .flags = NEXCACHEMODULE_CMD_ARG_OPTIONAL,
                .subargs = (NexCacheModuleCommandArg[]){
                    {
                        .name = "strategy",
                        .type = NEXCACHEMODULE_ARG_TYPE_ONEOF,
                        .subargs = (NexCacheModuleCommandArg[]){
                            {
                                .name = "maxlen",
                                .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                                .token = "MAXLEN",
                            },
                            {
                                .name = "minid",
                                .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                                .token = "MINID",
                                .since = "6.2.0",
                            },
                            {0}
                        }
                    },
                    {
                        .name = "operator",
                        .type = NEXCACHEMODULE_ARG_TYPE_ONEOF,
                        .flags = NEXCACHEMODULE_CMD_ARG_OPTIONAL,
                        .subargs = (NexCacheModuleCommandArg[]){
                            {
                                .name = "equal",
                                .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                                .token = "="
                            },
                            {
                                .name = "approximately",
                                .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                                .token = "~"
                            },
                            {0}
                        }
                    },
                    {
                        .name = "threshold",
                        .type = NEXCACHEMODULE_ARG_TYPE_STRING,
                        .display_text = "threshold" /* Just for coverage, doesn't have a visible effect */
                    },
                    {
                        .name = "count",
                        .type = NEXCACHEMODULE_ARG_TYPE_INTEGER,
                        .token = "LIMIT",
                        .since = "6.2.0",
                        .flags = NEXCACHEMODULE_CMD_ARG_OPTIONAL
                    },
                    {0}
                }
            },
            {
                .name = "id-selector",
                .type = NEXCACHEMODULE_ARG_TYPE_ONEOF,
                .subargs = (NexCacheModuleCommandArg[]){
                    {
                        .name = "auto-id",
                        .type = NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
                        .token = "*"
                    },
                    {
                        .name = "id",
                        .type = NEXCACHEMODULE_ARG_TYPE_STRING,
                    },
                    {0}
                }
            },
            {
                .name = "data",
                .type = NEXCACHEMODULE_ARG_TYPE_BLOCK,
                .flags = NEXCACHEMODULE_CMD_ARG_MULTIPLE,
                .subargs = (NexCacheModuleCommandArg[]){
                    {
                        .name = "field",
                        .type = NEXCACHEMODULE_ARG_TYPE_STRING,
                    },
                    {
                        .name = "value",
                        .type = NEXCACHEMODULE_ARG_TYPE_STRING,
                    },
                    {0}
                }
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(xadd, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
