#include "nexcachemodule.h"

#include <string.h>
#include <assert.h>
#include <unistd.h>

typedef struct {
    size_t nkeys;
} scan_strings_pd;

void scan_strings_callback(NexCacheModuleCtx *ctx, NexCacheModuleString* keyname, NexCacheModuleKey* key, void *privdata) {
    scan_strings_pd* pd = privdata;
    int was_opened = 0;
    if (!key) {
        key = NexCacheModule_OpenKey(ctx, keyname, NEXCACHEMODULE_READ);
        was_opened = 1;
    }

    if (NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_STRING) {
        size_t len;
        char * data = NexCacheModule_StringDMA(key, &len, NEXCACHEMODULE_READ);
        NexCacheModule_ReplyWithArray(ctx, 2);
        NexCacheModule_ReplyWithString(ctx, keyname);
        NexCacheModule_ReplyWithStringBuffer(ctx, data, len);
        pd->nkeys++;
    }
    if (was_opened)
        NexCacheModule_CloseKey(key);
}

int scan_strings(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    scan_strings_pd pd = {
        .nkeys = 0,
    };

    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);

    NexCacheModuleScanCursor* cursor = NexCacheModule_ScanCursorCreate();
    while(NexCacheModule_Scan(ctx, cursor, scan_strings_callback, &pd));
    NexCacheModule_ScanCursorDestroy(cursor);

    NexCacheModule_ReplySetArrayLength(ctx, pd.nkeys);
    return NEXCACHEMODULE_OK;
}

typedef struct {
    NexCacheModuleCtx *ctx;
    size_t nreplies;
} scan_key_pd;

void scan_key_callback(NexCacheModuleKey *key, NexCacheModuleString* field, NexCacheModuleString* value, void *privdata) {
    NEXCACHEMODULE_NOT_USED(key);
    scan_key_pd* pd = privdata;
    NexCacheModule_ReplyWithArray(pd->ctx, 2);
    size_t fieldCStrLen;

    // The implementation of NexCacheModuleString is robj with lots of encodings.
    // We want to make sure the robj that passes to this callback in
    // String encoded, this is why we use NexCacheModule_StringPtrLen and
    // NexCacheModule_ReplyWithStringBuffer instead of directly use
    // NexCacheModule_ReplyWithString.
    const char* fieldCStr = NexCacheModule_StringPtrLen(field, &fieldCStrLen);
    NexCacheModule_ReplyWithStringBuffer(pd->ctx, fieldCStr, fieldCStrLen);
    if(value){
        size_t valueCStrLen;
        const char* valueCStr = NexCacheModule_StringPtrLen(value, &valueCStrLen);
        NexCacheModule_ReplyWithStringBuffer(pd->ctx, valueCStr, valueCStrLen);
    } else {
        NexCacheModule_ReplyWithNull(pd->ctx);
    }

    pd->nreplies++;
}

int scan_key(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    scan_key_pd pd = {
        .ctx = ctx,
        .nreplies = 0,
    };

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    if (!key) {
        NexCacheModule_ReplyWithError(ctx, "not found");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_ARRAY_LEN);

    NexCacheModuleScanCursor* cursor = NexCacheModule_ScanCursorCreate();
    while(NexCacheModule_ScanKey(key, cursor, scan_key_callback, &pd));
    NexCacheModule_ScanCursorDestroy(cursor);

    NexCacheModule_ReplySetArrayLength(ctx, pd.nreplies);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "scan", 1, NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "scan.scan_strings", scan_strings, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "scan.scan_key", scan_key, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}


