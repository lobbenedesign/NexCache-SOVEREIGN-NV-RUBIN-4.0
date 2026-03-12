/* This module current tests a small subset but should be extended in the future
 * for general ModuleDataType coverage.
 */

/* define macros for having usleep */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <unistd.h>

#include "nexcachemodule.h"

static NexCacheModuleType *datatype = NULL;
static int load_encver = 0;

/* used to test processing events during slow loading */
static volatile int slow_loading = 0;
static volatile int is_in_slow_loading = 0;

#define DATATYPE_ENC_VER 1

typedef struct {
    long long intval;
    NexCacheModuleString *strval;
} DataType;

static void *datatype_load(NexCacheModuleIO *io, int encver) {
    load_encver = encver;
    int intval = NexCacheModule_LoadSigned(io);
    if (NexCacheModule_IsIOError(io)) return NULL;

    NexCacheModuleString *strval = NexCacheModule_LoadString(io);
    if (NexCacheModule_IsIOError(io)) return NULL;

    DataType *dt = (DataType *) NexCacheModule_Alloc(sizeof(DataType));
    dt->intval = intval;
    dt->strval = strval;

    if (slow_loading) {
        NexCacheModuleCtx *ctx = NexCacheModule_GetContextFromIO(io);
        is_in_slow_loading = 1;
        while (slow_loading) {
            NexCacheModule_Yield(ctx, NEXCACHEMODULE_YIELD_FLAG_CLIENTS, "Slow module operation");
            usleep(1000);
        }
        is_in_slow_loading = 0;
    }

    return dt;
}

static void datatype_save(NexCacheModuleIO *io, void *value) {
    DataType *dt = (DataType *) value;
    NexCacheModule_SaveSigned(io, dt->intval);
    NexCacheModule_SaveString(io, dt->strval);
}

static void datatype_free(void *value) {
    if (value) {
        DataType *dt = (DataType *) value;

        if (dt->strval) NexCacheModule_FreeString(NULL, dt->strval);
        NexCacheModule_Free(dt);
    }
}

static void *datatype_copy(NexCacheModuleString *fromkey, NexCacheModuleString *tokey, const void *value) {
    const DataType *old = value;

    /* Answers to ultimate questions cannot be copied! */
    if (old->intval == 42)
        return NULL;

    DataType *new = (DataType *) NexCacheModule_Alloc(sizeof(DataType));

    new->intval = old->intval;
    new->strval = NexCacheModule_CreateStringFromString(NULL, old->strval);

    /* Breaking the rules here! We return a copy that also includes traces
     * of fromkey/tokey to confirm we get what we expect.
     */
    size_t len;
    const char *str = NexCacheModule_StringPtrLen(fromkey, &len);
    NexCacheModule_StringAppendBuffer(NULL, new->strval, "/", 1);
    NexCacheModule_StringAppendBuffer(NULL, new->strval, str, len);
    NexCacheModule_StringAppendBuffer(NULL, new->strval, "/", 1);
    str = NexCacheModule_StringPtrLen(tokey, &len);
    NexCacheModule_StringAppendBuffer(NULL, new->strval, str, len);

    return new;
}

static int datatype_set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long intval;

    if (NexCacheModule_StringToLongLong(argv[2], &intval) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    DataType *dt = NexCacheModule_Calloc(sizeof(DataType), 1);
    dt->intval = intval;
    dt->strval = argv[3];
    NexCacheModule_RetainString(ctx, dt->strval);

    NexCacheModule_ModuleTypeSetValue(key, datatype, dt);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

    return NEXCACHEMODULE_OK;
}

static int datatype_restore(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long encver;
    if (NexCacheModule_StringToLongLong(argv[3], &encver) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }

    DataType *dt = NexCacheModule_LoadDataTypeFromStringEncver(argv[2], datatype, encver);
    if (!dt) {
        NexCacheModule_ReplyWithError(ctx, "Invalid data");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    NexCacheModule_ModuleTypeSetValue(key, datatype, dt);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithLongLong(ctx, load_encver);

    return NEXCACHEMODULE_OK;
}

static int datatype_get(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    DataType *dt = NexCacheModule_ModuleTypeGetValue(key);
    NexCacheModule_CloseKey(key);

    if (!dt) {
        NexCacheModule_ReplyWithNullArray(ctx);
    } else {
        NexCacheModule_ReplyWithArray(ctx, 2);
        NexCacheModule_ReplyWithLongLong(ctx, dt->intval);
        NexCacheModule_ReplyWithString(ctx, dt->strval);
    }
    return NEXCACHEMODULE_OK;
}

static int datatype_dump(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    DataType *dt = NexCacheModule_ModuleTypeGetValue(key);
    NexCacheModule_CloseKey(key);

    NexCacheModuleString *reply = NexCacheModule_SaveDataTypeToString(ctx, dt, datatype);
    if (!reply) {
        NexCacheModule_ReplyWithError(ctx, "Failed to save");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithString(ctx, reply);
    NexCacheModule_FreeString(ctx, reply);
    return NEXCACHEMODULE_OK;
}

static int datatype_swap(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *a = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    NexCacheModuleKey *b = NexCacheModule_OpenKey(ctx, argv[2], NEXCACHEMODULE_WRITE);
    void *val = NexCacheModule_ModuleTypeGetValue(a);

    int error = (NexCacheModule_ModuleTypeReplaceValue(b, datatype, val, &val) == NEXCACHEMODULE_ERR ||
                 NexCacheModule_ModuleTypeReplaceValue(a, datatype, val, NULL) == NEXCACHEMODULE_ERR);
    if (!error)
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    else
        NexCacheModule_ReplyWithError(ctx, "ERR failed");

    NexCacheModule_CloseKey(a);
    NexCacheModule_CloseKey(b);

    return NEXCACHEMODULE_OK;
}

/* used to enable or disable slow loading */
static int datatype_slow_loading(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long ll;
    if (NexCacheModule_StringToLongLong(argv[1], &ll) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }
    slow_loading = ll;
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* used to test if we reached the slow loading code */
static int datatype_is_in_slow_loading(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithLongLong(ctx, is_in_slow_loading);
    return NEXCACHEMODULE_OK;
}

int createDataTypeBlockCheck(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    static NexCacheModuleType *datatype_outside_onload = NULL;

    NexCacheModuleTypeMethods datatype_methods = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = datatype_load,
        .rdb_save = datatype_save,
        .free = datatype_free,
        .copy = datatype_copy
    };

    datatype_outside_onload = NexCacheModule_CreateDataType(ctx, "test_dt_outside_onload", 1, &datatype_methods);

    /* This validates that it's not possible to create datatype outside OnLoad,
     * thus returns an error if it succeeds. */
    if (datatype_outside_onload == NULL) {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        NexCacheModule_ReplyWithError(ctx, "UNEXPECTEDOK");
    }
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"datatype",DATATYPE_ENC_VER,NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Creates a command which creates a datatype outside OnLoad() function. */
    if (NexCacheModule_CreateCommand(ctx,"block.create.datatype.outside.onload", createDataTypeBlockCheck, "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_HANDLE_IO_ERRORS);

    NexCacheModuleTypeMethods datatype_methods = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = datatype_load,
        .rdb_save = datatype_save,
        .free = datatype_free,
        .copy = datatype_copy
    };

    datatype = NexCacheModule_CreateDataType(ctx, "test___dt", 1, &datatype_methods);
    if (datatype == NULL)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"datatype.set", datatype_set,
                                  "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"datatype.get", datatype_get,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"datatype.restore", datatype_restore,
                                  "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"datatype.dump", datatype_dump,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "datatype.swap", datatype_swap,
                                  "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "datatype.slow_loading", datatype_slow_loading,
                                  "allow-loading", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "datatype.is_in_slow_loading", datatype_is_in_slow_loading,
                                  "allow-loading", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
