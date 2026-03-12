#include "nexcachemodule.h"
#include <strings.h>

/* Second module configs module, for testing.
 * Need to make sure that multiple modules with configs don't interfere with each other */
int bool_config;

int getBoolConfigCommand(const char *name, void *privdata) {
    NEXCACHEMODULE_NOT_USED(privdata);
    if (!strcasecmp(name, "test")) {
        return bool_config;
    }
    return 0;
}

int setBoolConfigCommand(const char *name, int new, void *privdata, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(privdata);
    NEXCACHEMODULE_NOT_USED(err);
    if (!strcasecmp(name, "test")) {
        bool_config = new;
        return NEXCACHEMODULE_OK;
    }
    return NEXCACHEMODULE_ERR;
}

/* No arguments are expected */ 
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "configs", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_RegisterBoolConfig(ctx, "test", 1, NEXCACHEMODULE_CONFIG_DEFAULT, getBoolConfigCommand, setBoolConfigCommand, NULL, &argc) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    if (NexCacheModule_LoadConfigs(ctx) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    return NEXCACHEMODULE_OK;
}