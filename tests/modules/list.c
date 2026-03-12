#include "nexcachemodule.h"
#include <assert.h>
#include <errno.h>
#include <strings.h>

/* LIST.GETALL key [REVERSE] */
int list_getall(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 2 || argc > 3) return NexCacheModule_WrongArity(ctx);
    int reverse = (argc == 3 &&
                   !strcasecmp(NexCacheModule_StringPtrLen(argv[2], NULL),
                               "REVERSE"));
    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_LIST) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }
    long n = NexCacheModule_ValueLength(key);
    NexCacheModule_ReplyWithArray(ctx, n);
    if (!reverse) {
        for (long i = 0; i < n; i++) {
            NexCacheModuleString *elem = NexCacheModule_ListGet(key, i);
            NexCacheModule_ReplyWithString(ctx, elem);
            NexCacheModule_FreeString(ctx, elem);
        }
    } else {
        for (long i = -1; i >= -n; i--) {
            NexCacheModuleString *elem = NexCacheModule_ListGet(key, i);
            NexCacheModule_ReplyWithString(ctx, elem);
            NexCacheModule_FreeString(ctx, elem);
        }
    }

    /* Test error condition: index out of bounds */
    assert(NexCacheModule_ListGet(key, n) == NULL);
    assert(errno == EDOM); /* no more elements in list */

    /* NexCacheModule_CloseKey(key); //implicit, done by auto memory */
    return NEXCACHEMODULE_OK;
}

/* LIST.EDIT key [REVERSE] cmdstr [value ..]
 *
 * cmdstr is a string of the following characters:
 *
 *     k -- keep
 *     d -- delete
 *     i -- insert value from args
 *     r -- replace with value from args
 *
 * The number of occurrences of "i" and "r" in cmdstr) should correspond to the
 * number of args after cmdstr.
 *
 * Reply with a RESP3 Map, containing the number of edits (inserts, replaces, deletes)
 * performed, as well as the last index and the entry it points to.
 */
int list_edit(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx);
    int argpos = 1; /* the next arg */

    /* key */
    int keymode = NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[argpos++], keymode);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_LIST) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    /* REVERSE */
    int reverse = 0;
    if (argc >= 4 &&
        !strcasecmp(NexCacheModule_StringPtrLen(argv[argpos], NULL), "REVERSE")) {
        reverse = 1;
        argpos++;
    }

    /* cmdstr */
    size_t cmdstr_len;
    const char *cmdstr = NexCacheModule_StringPtrLen(argv[argpos++], &cmdstr_len);

    /* validate cmdstr vs. argc */
    long num_req_args = 0;
    long min_list_length = 0;
    for (size_t cmdpos = 0; cmdpos < cmdstr_len; cmdpos++) {
        char c = cmdstr[cmdpos];
        if (c == 'i' || c == 'r') num_req_args++;
        if (c == 'd' || c == 'r' || c == 'k') min_list_length++;
    }
    if (argc < argpos + num_req_args) {
        return NexCacheModule_ReplyWithError(ctx, "ERR too few args");
    }
    if ((long)NexCacheModule_ValueLength(key) < min_list_length) {
        return NexCacheModule_ReplyWithError(ctx, "ERR list too short");
    }

    /* Iterate over the chars in cmdstr (edit instructions) */
    long long num_inserts = 0, num_deletes = 0, num_replaces = 0;
    long index = reverse ? -1 : 0;
    NexCacheModuleString *value;

    for (size_t cmdpos = 0; cmdpos < cmdstr_len; cmdpos++) {
        switch (cmdstr[cmdpos]) {
        case 'i': /* insert */
            value = argv[argpos++];
            assert(NexCacheModule_ListInsert(key, index, value) == NEXCACHEMODULE_OK);
            index += reverse ? -1 : 1;
            num_inserts++;
            break;
        case 'd': /* delete */
            assert(NexCacheModule_ListDelete(key, index) == NEXCACHEMODULE_OK);
            num_deletes++;
            break;
        case 'r': /* replace */
            value = argv[argpos++];
            assert(NexCacheModule_ListSet(key, index, value) == NEXCACHEMODULE_OK);
            index += reverse ? -1 : 1;
            num_replaces++;
            break;
        case 'k': /* keep */
            index += reverse ? -1 : 1;
            break;
        }
    }

    NexCacheModuleString *v = NexCacheModule_ListGet(key, index);
    NexCacheModule_ReplyWithMap(ctx, v ? 5 : 4);
    NexCacheModule_ReplyWithCString(ctx, "i");
    NexCacheModule_ReplyWithLongLong(ctx, num_inserts);
    NexCacheModule_ReplyWithCString(ctx, "d");
    NexCacheModule_ReplyWithLongLong(ctx, num_deletes);
    NexCacheModule_ReplyWithCString(ctx, "r");
    NexCacheModule_ReplyWithLongLong(ctx, num_replaces);
    NexCacheModule_ReplyWithCString(ctx, "index");
    NexCacheModule_ReplyWithLongLong(ctx, index);
    if (v) {
        NexCacheModule_ReplyWithCString(ctx, "entry");
        NexCacheModule_ReplyWithString(ctx, v);
        NexCacheModule_FreeString(ctx, v);
    } 

    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* Reply based on errno as set by the List API functions. */
static int replyByErrno(NexCacheModuleCtx *ctx) {
    switch (errno) {
    case EDOM:
        return NexCacheModule_ReplyWithError(ctx, "ERR index out of bounds");
    case ENOTSUP:
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    default: assert(0); /* Can't happen */
    }
}

/* LIST.GET key index */
int list_get(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    long long index;
    if (NexCacheModule_StringToLongLong(argv[2], &index) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR index must be a number");
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    NexCacheModuleString *value = NexCacheModule_ListGet(key, index);
    if (value) {
        NexCacheModule_ReplyWithString(ctx, value);
        NexCacheModule_FreeString(ctx, value);
    } else {
        replyByErrno(ctx);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* LIST.SET key index value */
int list_set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    long long index;
    if (NexCacheModule_StringToLongLong(argv[2], &index) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "ERR index must be a number");
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_ListSet(key, index, argv[3]) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        replyByErrno(ctx);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* LIST.INSERT key index value
 *
 * If index is negative, value is inserted after, otherwise before the element
 * at index.
 */
int list_insert(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    long long index;
    if (NexCacheModule_StringToLongLong(argv[2], &index) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "ERR index must be a number");
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_ListInsert(key, index, argv[3]) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        replyByErrno(ctx);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* LIST.DELETE key index */
int list_delete(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    long long index;
    if (NexCacheModule_StringToLongLong(argv[2], &index) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "ERR index must be a number");
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_ListDelete(key, index) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        replyByErrno(ctx);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "list", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.getall", list_getall, "",
                                  1, 1, 1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.edit", list_edit, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.get", list_get, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.set", list_set, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.insert", list_insert, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK &&
        NexCacheModule_CreateCommand(ctx, "list.delete", list_delete, "write",
                                  1, 1, 1) == NEXCACHEMODULE_OK) {
        return NEXCACHEMODULE_OK;
    } else {
        return NEXCACHEMODULE_ERR;
    }
}
