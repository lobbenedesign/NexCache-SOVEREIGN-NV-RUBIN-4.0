#include "nexcachemodule.h"

#define UNUSED(V) ((void) V)

/* This function implements all commands in this module. All we care about is
 * the COMMAND metadata anyway. */
int kspec_impl(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    /* Handle getkeys-api introspection (for "kspec.nonewithgetkeys")  */
    if (NexCacheModule_IsKeysPositionRequest(ctx)) {
        for (int i = 1; i < argc; i += 2)
            NexCacheModule_KeyAtPosWithFlags(ctx, i, NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS);

        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int createKspecNone(NexCacheModuleCtx *ctx) {
    /* A command without keyspecs; only the legacy (first,last,step) triple (MSET like spec). */
    if (NexCacheModule_CreateCommand(ctx,"kspec.none",kspec_impl,"",1,-1,2) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}

int createKspecNoneWithGetkeys(NexCacheModuleCtx *ctx) {
    /* A command without keyspecs; only the legacy (first,last,step) triple (MSET like spec), but also has a getkeys callback */
    if (NexCacheModule_CreateCommand(ctx,"kspec.nonewithgetkeys",kspec_impl,"getkeys-api",1,-1,2) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}

int createKspecTwoRanges(NexCacheModuleCtx *ctx) {
    /* Test that two position/range-based key specs are combined to produce the
     * legacy (first,last,step) values representing both keys. */
    if (NexCacheModule_CreateCommand(ctx,"kspec.tworanges",kspec_impl,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *command = NexCacheModule_GetCommand(ctx,"kspec.tworanges");
    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .arity = -2,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 2,
                /* Omitted find_keys_type is shorthand for RANGE {0,1,0} */
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(command, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int createKspecTwoRangesWithGap(NexCacheModuleCtx *ctx) {
    /* Test that two position/range-based key specs are combined to produce the
     * legacy (first,last,step) values representing just one key. */
    if (NexCacheModule_CreateCommand(ctx,"kspec.tworangeswithgap",kspec_impl,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *command = NexCacheModule_GetCommand(ctx,"kspec.tworangeswithgap");
    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .arity = -2,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 3,
                /* Omitted find_keys_type is shorthand for RANGE {0,1,0} */
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(command, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int createKspecKeyword(NexCacheModuleCtx *ctx) {
    /* Only keyword-based specs. The legacy triple is wiped and set to (0,0,0). */
    if (NexCacheModule_CreateCommand(ctx,"kspec.keyword",kspec_impl,"",3,-1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *command = NexCacheModule_GetCommand(ctx,"kspec.keyword");
    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_KEYWORD,
                .bs.keyword.keyword = "KEYS",
                .bs.keyword.startfrom = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {-1,1,0}
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(command, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int createKspecComplex1(NexCacheModuleCtx *ctx) {
    /* First is a range a single key. The rest are keyword-based specs. */
    if (NexCacheModule_CreateCommand(ctx,"kspec.complex1",kspec_impl,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *command = NexCacheModule_GetCommand(ctx,"kspec.complex1");
    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_KEYWORD,
                .bs.keyword.keyword = "STORE",
                .bs.keyword.startfrom = 2,
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_KEYWORD,
                .bs.keyword.keyword = "KEYS",
                .bs.keyword.startfrom = 2,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_KEYNUM,
                .fk.keynum = {0,1,1}
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(command, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int createKspecComplex2(NexCacheModuleCtx *ctx) {
    /* First is not legacy, more than STATIC_KEYS_SPECS_NUM specs */
    if (NexCacheModule_CreateCommand(ctx,"kspec.complex2",kspec_impl,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleCommand *command = NexCacheModule_GetCommand(ctx,"kspec.complex2");
    NexCacheModuleCommandInfo info = {
        .version = NEXCACHEMODULE_COMMAND_INFO_VERSION,
        .key_specs = (NexCacheModuleCommandKeySpec[]){
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_KEYWORD,
                .bs.keyword.keyword = "STORE",
                .bs.keyword.startfrom = 5,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 1,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RO | NEXCACHEMODULE_CMD_KEY_ACCESS,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 2,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {0,1,0}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_INDEX,
                .bs.index.pos = 3,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_KEYNUM,
                .fk.keynum = {0,1,1}
            },
            {
                .flags = NEXCACHEMODULE_CMD_KEY_RW | NEXCACHEMODULE_CMD_KEY_UPDATE,
                .begin_search_type = NEXCACHEMODULE_KSPEC_BS_KEYWORD,
                .bs.keyword.keyword = "MOREKEYS",
                .bs.keyword.startfrom = 5,
                .find_keys_type = NEXCACHEMODULE_KSPEC_FK_RANGE,
                .fk.range = {-1,1,0}
            },
            {0}
        }
    };
    if (NexCacheModule_SetCommandInfo(command, &info) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "keyspecs", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (createKspecNone(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecNoneWithGetkeys(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecTwoRanges(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecTwoRangesWithGap(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecKeyword(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecComplex1(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    if (createKspecComplex2(ctx) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}
