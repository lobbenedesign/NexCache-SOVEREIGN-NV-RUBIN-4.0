#include "nexcachemodule.h"

#include <string.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

/* Command which adds a stream entry with automatic ID, like XADD *.
 *
 * Syntax: STREAM.ADD key field1 value1 [ field2 value2 ... ]
 *
 * The response is the ID of the added stream entry or an error message.
 */
int stream_add(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 2 || argc % 2 != 0) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    NexCacheModuleStreamID id;
    if (NexCacheModule_StreamAdd(key, NEXCACHEMODULE_STREAM_ADD_AUTOID, &id,
                              &argv[2], (argc-2)/2) == NEXCACHEMODULE_OK) {
        NexCacheModuleString *id_str = NexCacheModule_CreateStringFromStreamID(ctx, &id);
        NexCacheModule_ReplyWithString(ctx, id_str);
        NexCacheModule_FreeString(ctx, id_str);
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR StreamAdd failed");
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* Command which adds a stream entry N times.
 *
 * Syntax: STREAM.ADD key N field1 value1 [ field2 value2 ... ]
 *
 * Returns the number of successfully added entries.
 */
int stream_addn(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3 || argc % 2 == 0) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long n, i;
    if (NexCacheModule_StringToLongLong(argv[2], &n) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx, "N must be a number");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    for (i = 0; i < n; i++) {
        if (NexCacheModule_StreamAdd(key, NEXCACHEMODULE_STREAM_ADD_AUTOID, NULL,
                                  &argv[3], (argc-3)/2) == NEXCACHEMODULE_ERR)
            break;
    }
    NexCacheModule_ReplyWithLongLong(ctx, i);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* STREAM.DELETE key stream-id */
int stream_delete(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleStreamID id;
    if (NexCacheModule_StringToStreamID(argv[2], &id) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "Invalid stream ID");
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_StreamDelete(key, &id) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR StreamDelete failed");
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* STREAM.RANGE key start-id end-id
 *
 * Returns an array of stream items. Each item is an array on the form
 * [stream-id, [field1, value1, field2, value2, ...]].
 *
 * A funny side-effect used for testing RM_StreamIteratorDelete() is that if any
 * entry has a field named "selfdestruct", the stream entry is deleted. It is
 * however included in the results of this command.
 */
int stream_range(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleStreamID startid, endid;
    if (NexCacheModule_StringToStreamID(argv[2], &startid) != NEXCACHEMODULE_OK ||
        NexCacheModule_StringToStreamID(argv[3], &endid) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid stream ID");
        return NEXCACHEMODULE_OK;
    }

    /* If startid > endid, we swap and set the reverse flag. */
    int flags = 0;
    if (startid.ms > endid.ms ||
        (startid.ms == endid.ms && startid.seq > endid.seq)) {
        NexCacheModuleStreamID tmp = startid;
        startid = endid;
        endid = tmp;
        flags |= NEXCACHEMODULE_STREAM_ITERATOR_REVERSE;
    }

    /* Open key and start iterator. */
    int openflags = NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], openflags);
    if (NexCacheModule_StreamIteratorStart(key, flags,
                                        &startid, &endid) != NEXCACHEMODULE_OK) {
        /* Key is not a stream, etc. */
        NexCacheModule_ReplyWithError(ctx, "ERR StreamIteratorStart failed");
        NexCacheModule_CloseKey(key);
        return NEXCACHEMODULE_OK;
    }

    /* Check error handling: Delete current entry when no current entry. */
    assert(NexCacheModule_StreamIteratorDelete(key) ==
           NEXCACHEMODULE_ERR);
    assert(errno == ENOENT);

    /* Check error handling: Fetch fields when no current entry. */
    assert(NexCacheModule_StreamIteratorNextField(key, NULL, NULL) ==
           NEXCACHEMODULE_ERR);
    assert(errno == ENOENT);

    /* Return array. */
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleStreamID id;
    long numfields;
    long len = 0;
    while (NexCacheModule_StreamIteratorNextID(key, &id,
                                            &numfields) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithArray(ctx, 2);
        NexCacheModuleString *id_str = NexCacheModule_CreateStringFromStreamID(ctx, &id);
        NexCacheModule_ReplyWithString(ctx, id_str);
        NexCacheModule_ReplyWithArray(ctx, numfields * 2);
        int delete = 0;
        NexCacheModuleString *field, *value;
        for (long i = 0; i < numfields; i++) {
            assert(NexCacheModule_StreamIteratorNextField(key, &field, &value) ==
                   NEXCACHEMODULE_OK);
            NexCacheModule_ReplyWithString(ctx, field);
            NexCacheModule_ReplyWithString(ctx, value);
            /* check if this is a "selfdestruct" field */
            size_t field_len;
            const char *field_str = NexCacheModule_StringPtrLen(field, &field_len);
            if (!strncmp(field_str, "selfdestruct", field_len)) delete = 1;
        }
        if (delete) {
            assert(NexCacheModule_StreamIteratorDelete(key) == NEXCACHEMODULE_OK);
        }
        /* check error handling: no more fields to fetch */
        assert(NexCacheModule_StreamIteratorNextField(key, &field, &value) ==
               NEXCACHEMODULE_ERR);
        assert(errno == ENOENT);
        len++;
    }
    NexCacheModule_ReplySetArrayLength(ctx, len);
    NexCacheModule_StreamIteratorStop(key);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/*
 * STREAM.TRIM key (MAXLEN (=|~) length | MINID (=|~) id)
 */
int stream_trim(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 5) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    /* Parse args */
    int trim_by_id = 0; /* 0 = maxlen, 1 = minid */
    long long maxlen;
    NexCacheModuleStreamID minid;
    size_t arg_len;
    const char *arg = NexCacheModule_StringPtrLen(argv[2], &arg_len);
    if (!strcasecmp(arg, "minid")) {
        trim_by_id = 1;
        if (NexCacheModule_StringToStreamID(argv[4], &minid) != NEXCACHEMODULE_OK) {
            NexCacheModule_ReplyWithError(ctx, "ERR Invalid stream ID");
            return NEXCACHEMODULE_OK;
        }
    } else if (!strcasecmp(arg, "maxlen")) {
        if (NexCacheModule_StringToLongLong(argv[4], &maxlen) == NEXCACHEMODULE_ERR) {
            NexCacheModule_ReplyWithError(ctx, "ERR Maxlen must be a number");
            return NEXCACHEMODULE_OK;
        }
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR Invalid arguments");
        return NEXCACHEMODULE_OK;
    }

    /* Approx or exact */
    int flags;
    arg = NexCacheModule_StringPtrLen(argv[3], &arg_len);
    if (arg_len == 1 && arg[0] == '~') {
        flags = NEXCACHEMODULE_STREAM_TRIM_APPROX;
    } else if (arg_len == 1 && arg[0] == '=') {
        flags = 0;
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR Invalid approx-or-exact mark");
        return NEXCACHEMODULE_OK;
    }

    /* Trim */
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    long long trimmed;
    if (trim_by_id) {
        trimmed = NexCacheModule_StreamTrimByID(key, flags, &minid);
    } else {
        trimmed = NexCacheModule_StreamTrimByLength(key, flags, maxlen);
    }

    /* Return result */
    if (trimmed < 0) {
        NexCacheModule_ReplyWithError(ctx, "ERR Trimming failed");
    } else {
        NexCacheModule_ReplyWithLongLong(ctx, trimmed);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "stream", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "stream.add", stream_add, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "stream.addn", stream_addn, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "stream.delete", stream_delete, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "stream.range", stream_range, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "stream.trim", stream_trim, "write",
                                  1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
