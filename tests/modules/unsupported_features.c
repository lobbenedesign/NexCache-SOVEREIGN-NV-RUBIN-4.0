
#include "nexcachemodule.h"

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"unsupported_features",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* This module does not set any options, meaning it will not opt-in to
     * features like Atomic Slot Migration and Async Loading */

    return NEXCACHEMODULE_OK;
}