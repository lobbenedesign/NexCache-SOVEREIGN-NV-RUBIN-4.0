#include "nexcachemodule.h"
#include <strings.h>
int mutable_bool_val;
int immutable_bool_val;
long long longval;
long long memval;
NexCacheModuleString *strval = NULL;
int enumval;
int flagsval;

/* Series of get and set callbacks for each type of config, these rely on the privdata ptr
 * to point to the config, and they register the configs as such. Note that one could also just
 * use names if they wanted, and store anything in privdata. */
int getBoolConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(name);
    return (*(int *)privdata);
}

int setBoolConfigCommand(const char *name, int new, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(err);
    *(int *)privdata = new;
    return NEXCACHEMODULE_OK;
}

long long getNumericConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(name);
    return (*(long long *) privdata);
}

int setNumericConfigCommand(const char *name, long long new, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(err);
    *(long long *)privdata = new;
    return NEXCACHEMODULE_OK;
}

NexCacheModuleString *getStringConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(privdata);
    return strval;
}
int setStringConfigCommand(const char *name, NexCacheModuleString *new, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(err);
    NEXCACHEMODULE_NOT_USED(privdata);
    size_t len;
    if (!strcasecmp(NexCacheModule_StringPtrLen(new, &len), "rejectisfreed")) {
        *err = NexCacheModule_CreateString(NULL, "Cannot set string to 'rejectisfreed'", 36);
        return NEXCACHEMODULE_ERR;
    }
    if (strval) NexCacheModule_FreeString(NULL, strval);
    NexCacheModule_RetainString(NULL, new);
    strval = new;
    return NEXCACHEMODULE_OK;
}

int getEnumConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(privdata);
    return enumval;
}

int setEnumConfigCommand(const char *name, int val, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(err);
    NEXCACHEMODULE_NOT_USED(privdata);
    enumval = val;
    return NEXCACHEMODULE_OK;
}

int getFlagsConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(privdata);
    return flagsval;
}

int setFlagsConfigCommand(const char *name, int val, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(name);
    NEXCACHEMODULE_NOT_USED(err);
    NEXCACHEMODULE_NOT_USED(privdata);
    flagsval = val;
    return NEXCACHEMODULE_OK;
}

int boolApplyFunc(NexCacheModuleCtx *ctx, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(privdata);
    if (mutable_bool_val && immutable_bool_val) {
        *err = NexCacheModule_CreateString(NULL, "Bool configs cannot both be yes.", 32);
        return NEXCACHEMODULE_ERR;
    }
    return NEXCACHEMODULE_OK;
}

int longlongApplyFunc(NexCacheModuleCtx *ctx, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(privdata);
    if (longval == memval) {
        *err = NexCacheModule_CreateString(NULL, "These configs cannot equal each other.", 38);
        return NEXCACHEMODULE_ERR;
    }
    return NEXCACHEMODULE_OK;
}

int registerBlockCheck(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    int response_ok = 0;
    int result = NexCacheModule_RegisterBoolConfig(ctx, "mutable_bool", 1, NEXCACHEMODULE_CONFIG_DEFAULT, getBoolConfigCommand, setBoolConfigCommand, boolApplyFunc, &mutable_bool_val);
    response_ok |= (result == NEXCACHEMODULE_OK);

    result = NexCacheModule_RegisterStringConfig(ctx, "string", "secret password", NEXCACHEMODULE_CONFIG_DEFAULT, getStringConfigCommand, setStringConfigCommand, NULL, NULL);
    response_ok |= (result == NEXCACHEMODULE_OK);

    const char *enum_vals[] = {"none", "five", "one", "two", "four"};
    const int int_vals[] = {0, 5, 1, 2, 4};
    result = NexCacheModule_RegisterEnumConfig(ctx, "enum", 1, NEXCACHEMODULE_CONFIG_DEFAULT, enum_vals, int_vals, 5, getEnumConfigCommand, setEnumConfigCommand, NULL, NULL);
    response_ok |= (result == NEXCACHEMODULE_OK);

    result = NexCacheModule_RegisterNumericConfig(ctx, "numeric", -1, NEXCACHEMODULE_CONFIG_DEFAULT, -5, 2000, getNumericConfigCommand, setNumericConfigCommand, longlongApplyFunc, &longval);
    response_ok |= (result == NEXCACHEMODULE_OK);

    result = NexCacheModule_LoadConfigs(ctx);
    response_ok |= (result == NEXCACHEMODULE_OK);
    
    /* This validates that it's not possible to register/load configs outside OnLoad,
     * thus returns an error if they succeed. */
    if (response_ok) {
        NexCacheModule_ReplyWithError(ctx, "UNEXPECTEDOK");
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    }
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "moduleconfigs", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_RegisterBoolConfig(ctx, "mutable_bool", 1, NEXCACHEMODULE_CONFIG_DEFAULT, getBoolConfigCommand, setBoolConfigCommand, boolApplyFunc, &mutable_bool_val) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    /* Immutable config here. */
    if (NexCacheModule_RegisterBoolConfig(ctx, "immutable_bool", 0, NEXCACHEMODULE_CONFIG_IMMUTABLE, getBoolConfigCommand, setBoolConfigCommand, boolApplyFunc, &immutable_bool_val) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    if (NexCacheModule_RegisterStringConfig(ctx, "string", "secret password", NEXCACHEMODULE_CONFIG_DEFAULT, getStringConfigCommand, setStringConfigCommand, NULL, NULL) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    /* On the stack to make sure we're copying them. */
    const char *enum_vals[] = {"none", "five", "one", "two", "four"};
    const int int_vals[] = {0, 5, 1, 2, 4};

    if (NexCacheModule_RegisterEnumConfig(ctx, "enum", 1, NEXCACHEMODULE_CONFIG_DEFAULT, enum_vals, int_vals, 5, getEnumConfigCommand, setEnumConfigCommand, NULL, NULL) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    if (NexCacheModule_RegisterEnumConfig(ctx, "flags", 3, NEXCACHEMODULE_CONFIG_DEFAULT | NEXCACHEMODULE_CONFIG_BITFLAGS, enum_vals, int_vals, 5, getFlagsConfigCommand, setFlagsConfigCommand, NULL, NULL) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    /* Memory config here. */
    if (NexCacheModule_RegisterNumericConfig(ctx, "memory_numeric", 1024, NEXCACHEMODULE_CONFIG_DEFAULT | NEXCACHEMODULE_CONFIG_MEMORY, 0, 3000000, getNumericConfigCommand, setNumericConfigCommand, longlongApplyFunc, &memval) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    if (NexCacheModule_RegisterNumericConfig(ctx, "numeric", -1, NEXCACHEMODULE_CONFIG_DEFAULT, -5, 2000, getNumericConfigCommand, setNumericConfigCommand, longlongApplyFunc, &longval) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    size_t len;
    if (argc && !strcasecmp(NexCacheModule_StringPtrLen(argv[0], &len), "noload")) {
        return NEXCACHEMODULE_OK;
    } else if (NexCacheModule_LoadConfigs(ctx) == NEXCACHEMODULE_ERR) {
        if (strval) {
            NexCacheModule_FreeString(ctx, strval);
            strval = NULL;
        }
        return NEXCACHEMODULE_ERR;
    }
    /* Creates a command which registers configs outside OnLoad() function. */
    if (NexCacheModule_CreateCommand(ctx,"block.register.configs.outside.onload", registerBlockCheck, "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
  
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NEXCACHEMODULE_NOT_USED(ctx);
    if (strval) {
        NexCacheModule_FreeString(ctx, strval);
        strval = NULL;
    }
    return NEXCACHEMODULE_OK;
}
