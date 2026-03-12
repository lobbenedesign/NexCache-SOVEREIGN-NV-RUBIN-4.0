#include "nexcachemodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

int test_module_update_parameter(NexCacheModuleCtx *ctx,
                                 NexCacheModuleString **argv, int argc) {

  NexCacheModule_UpdateRuntimeArgs(ctx, argv, argc);
  return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "moduleparameter", 1, NEXCACHEMODULE_APIVER_1) ==
        NEXCACHEMODULE_ERR)
      return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "testmoduleparameter.update.parameter",
                                   test_module_update_parameter, "fast", 0, 0,
                                   0) == NEXCACHEMODULE_ERR)
      return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
