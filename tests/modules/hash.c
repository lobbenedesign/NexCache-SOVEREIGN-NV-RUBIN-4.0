#include "nexcachemodule.h"
#include <strings.h>
#include <errno.h>
#include <stdlib.h>

/* If a string is ":deleted:", the special value for deleted hash fields is
 * returned; otherwise the input string is returned. */
static NexCacheModuleString *value_or_delete(NexCacheModuleString *s) {
    if (!strcasecmp(NexCacheModule_StringPtrLen(s, NULL), ":delete:"))
        return NEXCACHEMODULE_HASH_DELETE;
    else
        return s;
}

/* HASH.SET key flags field1 value1 [field2 value2 ..]
 *
 * Sets 1-4 fields. Returns the same as NexCacheModule_HashSet().
 * Flags is a string of "nxa" where n = NX, x = XX, a = COUNT_ALL.
 * To delete a field, use the value ":delete:".
 */
int hash_set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 5 || argc % 2 == 0 || argc > 11)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);

    size_t flags_len;
    const char *flags_str = NexCacheModule_StringPtrLen(argv[2], &flags_len);
    int flags = NEXCACHEMODULE_HASH_NONE;
    for (size_t i = 0; i < flags_len; i++) {
        switch (flags_str[i]) {
        case 'n': flags |= NEXCACHEMODULE_HASH_NX; break;
        case 'x': flags |= NEXCACHEMODULE_HASH_XX; break;
        case 'a': flags |= NEXCACHEMODULE_HASH_COUNT_ALL; break;
        }
    }

    /* Test some varargs. (In real-world, use a loop and set one at a time.) */
    int result;
    errno = 0;
    if (argc == 5) {
        result = NexCacheModule_HashSet(key, flags,
                                     argv[3], value_or_delete(argv[4]),
                                     NULL);
    } else if (argc == 7) {
        result = NexCacheModule_HashSet(key, flags,
                                     argv[3], value_or_delete(argv[4]),
                                     argv[5], value_or_delete(argv[6]),
                                     NULL);
    } else if (argc == 9) {
        result = NexCacheModule_HashSet(key, flags,
                                     argv[3], value_or_delete(argv[4]),
                                     argv[5], value_or_delete(argv[6]),
                                     argv[7], value_or_delete(argv[8]),
                                     NULL);
    } else if (argc == 11) {
        result = NexCacheModule_HashSet(key, flags,
                                     argv[3], value_or_delete(argv[4]),
                                     argv[5], value_or_delete(argv[6]),
                                     argv[7], value_or_delete(argv[8]),
                                     argv[9], value_or_delete(argv[10]),
                                     NULL);
    } else {
        return NexCacheModule_ReplyWithError(ctx, "ERR too many fields");
    }

    /* Check errno */
    if (result == 0) {
        if (errno == ENOTSUP)
            return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
        else
            NexCacheModule_Assert(errno == ENOENT);
    }

    return NexCacheModule_ReplyWithLongLong(ctx, result);
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "hash", 1, NEXCACHEMODULE_APIVER_1) ==
        NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "hash.set", hash_set, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK) {
        return NEXCACHEMODULE_OK;
    } else {
        return NEXCACHEMODULE_ERR;
    }
}
