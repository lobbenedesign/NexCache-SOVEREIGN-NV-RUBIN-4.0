#include "nexcachemodule.h"

#include <string.h>
#include <assert.h>

/* Module configuration, save aux or not? */
#define CONF_AUX_OPTION_NO_AUX           0
#define CONF_AUX_OPTION_SAVE2            1 << 0
#define CONF_AUX_OPTION_BEFORE_KEYSPACE  1 << 1
#define CONF_AUX_OPTION_AFTER_KEYSPACE   1 << 2
#define CONF_AUX_OPTION_NO_DATA          1 << 3
long long conf_aux_count = 0;

/* Registered type */
NexCacheModuleType *testrdb_type = NULL;

/* Global values to store and persist to aux */
NexCacheModuleString *before_str = NULL;
NexCacheModuleString *after_str = NULL;

/* Global values used to keep aux from db being loaded (in case of async_loading) */
NexCacheModuleString *before_str_temp = NULL;
NexCacheModuleString *after_str_temp = NULL;

/* Indicates whether there is an async replication in progress.
 * We control this value from NexCacheModuleEvent_ReplAsyncLoad events. */
int async_loading = 0;

int n_aux_load_called = 0;

void replAsyncLoadCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    switch (sub) {
    case NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_STARTED:
        assert(async_loading == 0);
        async_loading = 1;
        break;
    case NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_ABORTED:
        /* Discard temp aux */
        if (before_str_temp)
            NexCacheModule_FreeString(ctx, before_str_temp);
        if (after_str_temp)
            NexCacheModule_FreeString(ctx, after_str_temp);
        before_str_temp = NULL;
        after_str_temp = NULL;

        async_loading = 0;
        break;
    case NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_COMPLETED:
        if (before_str)
            NexCacheModule_FreeString(ctx, before_str);
        if (after_str)
            NexCacheModule_FreeString(ctx, after_str);
        before_str = before_str_temp;
        after_str = after_str_temp;

        before_str_temp = NULL;
        after_str_temp = NULL;

        async_loading = 0;
        break;
    default:
        assert(0);
    }
}

void *testrdb_type_load(NexCacheModuleIO *rdb, int encver) {
    int count = NexCacheModule_LoadSigned(rdb);
    NexCacheModuleString *str = NexCacheModule_LoadString(rdb);
    float f = NexCacheModule_LoadFloat(rdb);
    long double ld = NexCacheModule_LoadLongDouble(rdb);
    /* Context creation is part of the test. Creating the context will force the
     * core to allocate a context, which needs to be cleaned up when the
     * NexCacheModuleIO is destructed. */
    NexCacheModuleCtx *ctx = NexCacheModule_GetContextFromIO(rdb);
    if (NexCacheModule_IsIOError(rdb)) {
        if (str)
            NexCacheModule_FreeString(ctx, str);
        return NULL;
    }
    /* Using the values only after checking for io errors. */
    assert(count==1);
    assert(encver==1);
    assert(f==1.5f);
    assert(ld==0.333333333333333333L);
    return str;
}

void testrdb_type_save(NexCacheModuleIO *rdb, void *value) {
    NexCacheModuleString *str = (NexCacheModuleString*)value;
    NexCacheModule_SaveSigned(rdb, 1);
    NexCacheModule_SaveString(rdb, str);
    NexCacheModule_SaveFloat(rdb, 1.5);
    NexCacheModule_SaveLongDouble(rdb, 0.333333333333333333L);
}

void testrdb_aux_save(NexCacheModuleIO *rdb, int when) {
    if (!(conf_aux_count & CONF_AUX_OPTION_BEFORE_KEYSPACE)) assert(when == NEXCACHEMODULE_AUX_AFTER_RDB);
    if (!(conf_aux_count & CONF_AUX_OPTION_AFTER_KEYSPACE)) assert(when == NEXCACHEMODULE_AUX_BEFORE_RDB);
    assert(conf_aux_count!=CONF_AUX_OPTION_NO_AUX);
    /* Context creation is part of the test. Creating the context will force the
     * core to allocate a context, which needs to be cleaned up when the
     * NexCacheModuleIO is destructed. */
    NexCacheModuleCtx *ctx = NexCacheModule_GetContextFromIO(rdb);
    NEXCACHEMODULE_NOT_USED(ctx);
    if (when == NEXCACHEMODULE_AUX_BEFORE_RDB) {
        if (before_str) {
            NexCacheModule_SaveSigned(rdb, 1);
            NexCacheModule_SaveString(rdb, before_str);
        } else {
            NexCacheModule_SaveSigned(rdb, 0);
        }
    } else {
        if (after_str) {
            NexCacheModule_SaveSigned(rdb, 1);
            NexCacheModule_SaveString(rdb, after_str);
        } else {
            NexCacheModule_SaveSigned(rdb, 0);
        }
    }
}

int testrdb_aux_load(NexCacheModuleIO *rdb, int encver, int when) {
    assert(encver == 1);
    if (!(conf_aux_count & CONF_AUX_OPTION_BEFORE_KEYSPACE)) assert(when == NEXCACHEMODULE_AUX_AFTER_RDB);
    if (!(conf_aux_count & CONF_AUX_OPTION_AFTER_KEYSPACE)) assert(when == NEXCACHEMODULE_AUX_BEFORE_RDB);
    assert(conf_aux_count!=CONF_AUX_OPTION_NO_AUX);
    NexCacheModuleCtx *ctx = NexCacheModule_GetContextFromIO(rdb);
    if (when == NEXCACHEMODULE_AUX_BEFORE_RDB) {
        if (async_loading == 0) {
            if (before_str)
                NexCacheModule_FreeString(ctx, before_str);
            before_str = NULL;
            int count = NexCacheModule_LoadSigned(rdb);
            if (NexCacheModule_IsIOError(rdb))
                return NEXCACHEMODULE_ERR;
            if (count)
                before_str = NexCacheModule_LoadString(rdb);
        } else {
            if (before_str_temp)
                NexCacheModule_FreeString(ctx, before_str_temp);
            before_str_temp = NULL;
            int count = NexCacheModule_LoadSigned(rdb);
            if (NexCacheModule_IsIOError(rdb))
                return NEXCACHEMODULE_ERR;
            if (count)
                before_str_temp = NexCacheModule_LoadString(rdb);
        }
    } else {
        if (async_loading == 0) {
            if (after_str)
                NexCacheModule_FreeString(ctx, after_str);
            after_str = NULL;
            int count = NexCacheModule_LoadSigned(rdb);
            if (NexCacheModule_IsIOError(rdb))
                return NEXCACHEMODULE_ERR;
            if (count)
                after_str = NexCacheModule_LoadString(rdb);
        } else {
            if (after_str_temp)
                NexCacheModule_FreeString(ctx, after_str_temp);
            after_str_temp = NULL;
            int count = NexCacheModule_LoadSigned(rdb);
            if (NexCacheModule_IsIOError(rdb))
                return NEXCACHEMODULE_ERR;
            if (count)
                after_str_temp = NexCacheModule_LoadString(rdb);
        }
    }

    if (NexCacheModule_IsIOError(rdb))
        return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}

void testrdb_type_free(void *value) {
    if (value)
        NexCacheModule_FreeString(NULL, (NexCacheModuleString*)value);
}

int testrdb_set_before(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    if (before_str)
        NexCacheModule_FreeString(ctx, before_str);
    before_str = argv[1];
    NexCacheModule_RetainString(ctx, argv[1]);
    NexCacheModule_ReplyWithLongLong(ctx, 1);
    return NEXCACHEMODULE_OK;
}

int testrdb_get_before(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    if (before_str)
        NexCacheModule_ReplyWithString(ctx, before_str);
    else
        NexCacheModule_ReplyWithStringBuffer(ctx, "", 0);
    return NEXCACHEMODULE_OK;
}

/* For purpose of testing module events, expose variable state during async_loading. */
int testrdb_async_loading_get_before(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    if (before_str_temp)
        NexCacheModule_ReplyWithString(ctx, before_str_temp);
    else
        NexCacheModule_ReplyWithStringBuffer(ctx, "", 0);
    return NEXCACHEMODULE_OK;
}

int testrdb_set_after(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    if (after_str)
        NexCacheModule_FreeString(ctx, after_str);
    after_str = argv[1];
    NexCacheModule_RetainString(ctx, argv[1]);
    NexCacheModule_ReplyWithLongLong(ctx, 1);
    return NEXCACHEMODULE_OK;
}

int testrdb_get_after(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    if (after_str)
        NexCacheModule_ReplyWithString(ctx, after_str);
    else
        NexCacheModule_ReplyWithStringBuffer(ctx, "", 0);
    return NEXCACHEMODULE_OK;
}

int testrdb_set_key(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 3){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    NexCacheModuleString *str = NexCacheModule_ModuleTypeGetValue(key);
    if (str)
        NexCacheModule_FreeString(ctx, str);
    NexCacheModule_ModuleTypeSetValue(key, testrdb_type, argv[2]);
    NexCacheModule_RetainString(ctx, argv[2]);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithLongLong(ctx, 1);
    return NEXCACHEMODULE_OK;
}

int testrdb_get_key(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    NexCacheModuleString *str = NexCacheModule_ModuleTypeGetValue(key);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithString(ctx, str);
    return NEXCACHEMODULE_OK;
}

int testrdb_get_n_aux_load_called(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_ReplyWithLongLong(ctx, n_aux_load_called);
    return NEXCACHEMODULE_OK;
}

int test2rdb_aux_load(NexCacheModuleIO *rdb, int encver, int when) {
    NEXCACHEMODULE_NOT_USED(rdb);
    NEXCACHEMODULE_NOT_USED(encver);
    NEXCACHEMODULE_NOT_USED(when);
    n_aux_load_called++;
    return NEXCACHEMODULE_OK;
}

void test2rdb_aux_save(NexCacheModuleIO *rdb, int when) {
    NEXCACHEMODULE_NOT_USED(rdb);
    NEXCACHEMODULE_NOT_USED(when);
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"testrdb",1,NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_HANDLE_IO_ERRORS | NEXCACHEMODULE_OPTIONS_HANDLE_REPL_ASYNC_LOAD | NEXCACHEMODULE_OPTIONS_HANDLE_ATOMIC_SLOT_MIGRATION);

    if (argc > 0)
        NexCacheModule_StringToLongLong(argv[0], &conf_aux_count);

    if (conf_aux_count==CONF_AUX_OPTION_NO_AUX) {
        NexCacheModuleTypeMethods datatype_methods = {
            .version = 1,
            .rdb_load = testrdb_type_load,
            .rdb_save = testrdb_type_save,
            .aof_rewrite = NULL,
            .digest = NULL,
            .free = testrdb_type_free,
        };

        testrdb_type = NexCacheModule_CreateDataType(ctx, "test__rdb", 1, &datatype_methods);
        if (testrdb_type == NULL)
            return NEXCACHEMODULE_ERR;
    } else if (!(conf_aux_count & CONF_AUX_OPTION_NO_DATA)) {
        NexCacheModuleTypeMethods datatype_methods = {
            .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
            .rdb_load = testrdb_type_load,
            .rdb_save = testrdb_type_save,
            .aof_rewrite = NULL,
            .digest = NULL,
            .free = testrdb_type_free,
            .aux_load = testrdb_aux_load,
            .aux_save = testrdb_aux_save,
            .aux_save_triggers = ((conf_aux_count & CONF_AUX_OPTION_BEFORE_KEYSPACE) ? NEXCACHEMODULE_AUX_BEFORE_RDB : 0) |
                                 ((conf_aux_count & CONF_AUX_OPTION_AFTER_KEYSPACE)  ? NEXCACHEMODULE_AUX_AFTER_RDB : 0)
        };

        if (conf_aux_count & CONF_AUX_OPTION_SAVE2) {
            datatype_methods.aux_save2 = testrdb_aux_save;
        }

        testrdb_type = NexCacheModule_CreateDataType(ctx, "test__rdb", 1, &datatype_methods);
        if (testrdb_type == NULL)
            return NEXCACHEMODULE_ERR;
    } else {

        /* Used to verify that aux_save2 api without any data, saves nothing to the RDB. */
        NexCacheModuleTypeMethods datatype_methods = {
            .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
            .aux_load = test2rdb_aux_load,
            .aux_save = test2rdb_aux_save,
            .aux_save_triggers = ((conf_aux_count & CONF_AUX_OPTION_BEFORE_KEYSPACE) ? NEXCACHEMODULE_AUX_BEFORE_RDB : 0) |
                                 ((conf_aux_count & CONF_AUX_OPTION_AFTER_KEYSPACE)  ? NEXCACHEMODULE_AUX_AFTER_RDB : 0)
        };
        if (conf_aux_count & CONF_AUX_OPTION_SAVE2) {
            datatype_methods.aux_save2 = test2rdb_aux_save;
        }

        NexCacheModule_CreateDataType(ctx, "test__rdb", 1, &datatype_methods);
    }

    if (NexCacheModule_CreateCommand(ctx,"testrdb.set.before", testrdb_set_before,"deny-oom",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.get.before", testrdb_get_before,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.async_loading.get.before", testrdb_async_loading_get_before,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.set.after", testrdb_set_after,"deny-oom",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.get.after", testrdb_get_after,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.set.key", testrdb_set_key,"deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.get.key", testrdb_get_key,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testrdb.get.n_aux_load_called", testrdb_get_n_aux_load_called,"",1,1,1) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_ReplAsyncLoad, replAsyncLoadCallback);

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    if (before_str)
        NexCacheModule_FreeString(ctx, before_str);
    if (after_str)
        NexCacheModule_FreeString(ctx, after_str);
    if (before_str_temp)
        NexCacheModule_FreeString(ctx, before_str_temp);
    if (after_str_temp)
        NexCacheModule_FreeString(ctx, after_str_temp);
    return NEXCACHEMODULE_OK;
}
