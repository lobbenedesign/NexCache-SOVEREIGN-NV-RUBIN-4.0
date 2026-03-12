#include "nexcachemodule.h"
#include <math.h>
#include <errno.h>

#define UNUSED(V) ((void) V)

/* ZSET.REM key element
 *
 * Removes an occurrence of an element from a sorted set. Replies with the
 * number of removed elements (0 or 1).
 */
int zset_rem(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx);
    int keymode = NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], keymode);
    int deleted;
    if (NexCacheModule_ZsetRem(key, argv[2], &deleted) == NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithLongLong(ctx, deleted);
    else
        return NexCacheModule_ReplyWithError(ctx, "ERR ZsetRem failed");
}

/* ZSET.ADD key score member
 *
 * Adds a specified member with the specified score to the sorted
 * set stored at key.
 */
int zset_add(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx);
    int keymode = NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], keymode);

    size_t len;
    double score;
    char *endptr;
    const char *str = NexCacheModule_StringPtrLen(argv[2], &len);
    score = strtod(str, &endptr);
    if (*endptr != '\0' || errno == ERANGE)
        return NexCacheModule_ReplyWithError(ctx, "value is not a valid float");

    if (NexCacheModule_ZsetAdd(key, score, argv[3], NULL) == NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    else
        return NexCacheModule_ReplyWithError(ctx, "ERR ZsetAdd failed");
}

/* ZSET.INCRBY key member increment
 *
 * Increments the score stored at member in the sorted set stored at key by increment.
 * Replies with the new score of this element.
 */
int zset_incrby(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx);
    int keymode = NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], keymode);

    size_t len;
    double score, newscore;
    char *endptr;
    const char *str = NexCacheModule_StringPtrLen(argv[3], &len);
    score = strtod(str, &endptr);
    if (*endptr != '\0' || errno == ERANGE)
        return NexCacheModule_ReplyWithError(ctx, "value is not a valid float");

    if (NexCacheModule_ZsetIncrby(key, score, argv[2], NULL, &newscore) == NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithDouble(ctx, newscore);
    else
        return NexCacheModule_ReplyWithError(ctx, "ERR ZsetIncrby failed");
}

static int zset_internal_rangebylex(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int reverse) {
    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_ZSET) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    if (reverse) {
        if (NexCacheModule_ZsetLastInLexRange(key, argv[2], argv[3]) != NEXCACHEMODULE_OK) {
            return NexCacheModule_ReplyWithError(ctx, "invalid range");
        }
    } else {
        if (NexCacheModule_ZsetFirstInLexRange(key, argv[2], argv[3]) != NEXCACHEMODULE_OK) {
            return NexCacheModule_ReplyWithError(ctx, "invalid range");
        }
    }

    int arraylen = 0;
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    while (!NexCacheModule_ZsetRangeEndReached(key)) {
        NexCacheModuleString *ele = NexCacheModule_ZsetRangeCurrentElement(key, NULL);
        NexCacheModule_ReplyWithString(ctx, ele);
        NexCacheModule_FreeString(ctx, ele);
        if (reverse) {
            NexCacheModule_ZsetRangePrev(key);
        } else {
            NexCacheModule_ZsetRangeNext(key);
        }
        arraylen += 1;
    }
    NexCacheModule_ZsetRangeStop(key);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplySetArrayLength(ctx, arraylen);
    return NEXCACHEMODULE_OK;
}

/* ZSET.rangebylex key min max
 *
 * Returns members in a sorted set within a lexicographical range.
 */
int zset_rangebylex(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4)
        return NexCacheModule_WrongArity(ctx);
    return zset_internal_rangebylex(ctx, argv, 0);
}

/* ZSET.revrangebylex key min max
 *
 * Returns members in a sorted set within a lexicographical range in reverse order.
 */
int zset_revrangebylex(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4)
        return NexCacheModule_WrongArity(ctx);
    return zset_internal_rangebylex(ctx, argv, 1);
}

static void zset_members_cb(NexCacheModuleKey *key, NexCacheModuleString *field, NexCacheModuleString *value, void *privdata) {
    UNUSED(key);
    UNUSED(value);
    NexCacheModuleCtx *ctx = (NexCacheModuleCtx *)privdata;
    NexCacheModule_ReplyWithString(ctx, field);
}

/* ZSET.members key
 *
 * Returns members in a sorted set.
 */
int zset_members(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_ZSET) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    NexCacheModule_ReplyWithArray(ctx, NexCacheModule_ValueLength(key));
    NexCacheModuleScanCursor *c = NexCacheModule_ScanCursorCreate();
    while (NexCacheModule_ScanKey(key, c, zset_members_cb, ctx));
    NexCacheModule_CloseKey(key);
    NexCacheModule_ScanCursorDestroy(c);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "zset", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.rem", zset_rem, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.add", zset_add, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.incrby", zset_incrby, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.rangebylex", zset_rangebylex, "readonly",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.revrangebylex", zset_revrangebylex, "readonly",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "zset.members", zset_members, "readonly",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
