#include "nexcachemodule.h"

#include <strings.h>
#include <sys/mman.h>

#define UNUSED(V) ((void) V)

void assertCrash(NexCacheModuleInfoCtx *ctx, int for_crash_report) {
    UNUSED(ctx);
    UNUSED(for_crash_report);
    NexCacheModule_Assert(0);
}

void segfaultCrash(NexCacheModuleInfoCtx *ctx, int for_crash_report) {
    UNUSED(ctx);
    UNUSED(for_crash_report);
    /* Compiler gives warnings about writing to a random address
     * e.g "*((char*)-1) = 'x';". As a workaround, we map a read-only area
     * and try to write there to trigger segmentation fault. */
    char *p = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    *p = 'x';
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"infocrash",1,NEXCACHEMODULE_APIVER_1)
            == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    NexCacheModule_Assert(argc == 1);
    if (!strcasecmp(NexCacheModule_StringPtrLen(argv[0], NULL), "segfault")) {
        if (NexCacheModule_RegisterInfoFunc(ctx, segfaultCrash) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    } else if(!strcasecmp(NexCacheModule_StringPtrLen(argv[0], NULL), "assert")) {
        if (NexCacheModule_RegisterInfoFunc(ctx, assertCrash) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;
    } else {
        return NEXCACHEMODULE_ERR;
    }

    return NEXCACHEMODULE_OK;
}
