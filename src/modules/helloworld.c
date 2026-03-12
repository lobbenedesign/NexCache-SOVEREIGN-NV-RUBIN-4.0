/* Helloworld module -- A few examples of the Modules API in the form
 * of commands showing how to accomplish common tasks.
 *
 * This module does not do anything useful, if not for a few commands. The
 * examples are designed in order to show the API.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2016, NexCache Contributors.
 * All rights reserved.
 *
 * NexCachetribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * NexCachetributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * NexCachetributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NexCache nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../nexcachemodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* HELLO.SIMPLE is among the simplest commands you can implement.
 * It just returns the currently selected DB id, a functionality which is
 * missing in the server. The command uses two important API calls: one to
 * fetch the currently selected DB, the other in order to send the client
 * an integer reply as response. */
int HelloSimple_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_ReplyWithLongLong(ctx, NexCacheModule_GetSelectedDb(ctx));
    return NEXCACHEMODULE_OK;
}

/* HELLO.PUSH.NATIVE re-implements RPUSH, and shows the low level modules API
 * where you can "open" keys, make low level operations, create new keys by
 * pushing elements into non-existing keys, and so forth.
 *
 * You'll find this command to be roughly as fast as the actual RPUSH
 * command. */
int HelloPushNative_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);

    NexCacheModule_ListPush(key, NEXCACHEMODULE_LIST_TAIL, argv[2]);
    size_t newlen = NexCacheModule_ValueLength(key);
    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithLongLong(ctx, newlen);
    return NEXCACHEMODULE_OK;
}

/* HELLO.PUSH.CALL implements RPUSH using an higher level approach, calling
 * a command instead of working with the key in a low level way. This
 * approach is useful when you need to call commands that are not
 * available as low level APIs, or when you don't need the maximum speed
 * possible but instead prefer implementation simplicity. */
int HelloPushCall_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx, "RPUSH", "ss", argv[1], argv[2]);
    long long len = NexCacheModule_CallReplyInteger(reply);
    NexCacheModule_FreeCallReply(reply);
    NexCacheModule_ReplyWithLongLong(ctx, len);
    return NEXCACHEMODULE_OK;
}

/* HELLO.PUSH.CALL2
 * This is exactly as HELLO.PUSH.CALL, but shows how we can reply to the
 * client using directly a reply object that Call() returned. */
int HelloPushCall2_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx, "RPUSH", "ss", argv[1], argv[2]);
    NexCacheModule_ReplyWithCallReply(ctx, reply);
    NexCacheModule_FreeCallReply(reply);
    return NEXCACHEMODULE_OK;
}

/* HELLO.LIST.SUM.LEN returns the total length of all the items inside
 * a list, by using the high level Call() API.
 * This command is an example of the array reply access. */
int HelloListSumLen_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx, "LRANGE", "sll", argv[1], (long long)0, (long long)-1);
    size_t strlen = 0;
    size_t items = NexCacheModule_CallReplyLength(reply);
    size_t j;
    for (j = 0; j < items; j++) {
        NexCacheModuleCallReply *ele = NexCacheModule_CallReplyArrayElement(reply, j);
        strlen += NexCacheModule_CallReplyLength(ele);
    }
    NexCacheModule_FreeCallReply(reply);
    NexCacheModule_ReplyWithLongLong(ctx, strlen);
    return NEXCACHEMODULE_OK;
}

/* HELLO.LIST.SPLICE srclist dstlist count
 * Moves 'count' elements from the tail of 'srclist' to the head of
 * 'dstlist'. If less than count elements are available, it moves as much
 * elements as possible. */
int HelloListSplice_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *srckey = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    NexCacheModuleKey *dstkey = NexCacheModule_OpenKey(ctx, argv[2], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);

    /* Src and dst key must be empty or lists. */
    if ((NexCacheModule_KeyType(srckey) != NEXCACHEMODULE_KEYTYPE_LIST &&
         NexCacheModule_KeyType(srckey) != NEXCACHEMODULE_KEYTYPE_EMPTY) ||
        (NexCacheModule_KeyType(dstkey) != NEXCACHEMODULE_KEYTYPE_LIST &&
         NexCacheModule_KeyType(dstkey) != NEXCACHEMODULE_KEYTYPE_EMPTY)) {
        NexCacheModule_CloseKey(srckey);
        NexCacheModule_CloseKey(dstkey);
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    long long count;
    if ((NexCacheModule_StringToLongLong(argv[3], &count) != NEXCACHEMODULE_OK) || (count < 0)) {
        NexCacheModule_CloseKey(srckey);
        NexCacheModule_CloseKey(dstkey);
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");
    }

    while (count-- > 0) {
        NexCacheModuleString *ele;

        ele = NexCacheModule_ListPop(srckey, NEXCACHEMODULE_LIST_TAIL);
        if (ele == NULL) break;
        NexCacheModule_ListPush(dstkey, NEXCACHEMODULE_LIST_HEAD, ele);
        NexCacheModule_FreeString(ctx, ele);
    }

    size_t len = NexCacheModule_ValueLength(srckey);
    NexCacheModule_CloseKey(srckey);
    NexCacheModule_CloseKey(dstkey);
    NexCacheModule_ReplyWithLongLong(ctx, len);
    return NEXCACHEMODULE_OK;
}

/* Like the HELLO.LIST.SPLICE above, but uses automatic memory management
 * in order to avoid freeing stuff. */
int HelloListSpliceAuto_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);

    NexCacheModule_AutoMemory(ctx);

    NexCacheModuleKey *srckey = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    NexCacheModuleKey *dstkey = NexCacheModule_OpenKey(ctx, argv[2], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);

    /* Src and dst key must be empty or lists. */
    if ((NexCacheModule_KeyType(srckey) != NEXCACHEMODULE_KEYTYPE_LIST &&
         NexCacheModule_KeyType(srckey) != NEXCACHEMODULE_KEYTYPE_EMPTY) ||
        (NexCacheModule_KeyType(dstkey) != NEXCACHEMODULE_KEYTYPE_LIST &&
         NexCacheModule_KeyType(dstkey) != NEXCACHEMODULE_KEYTYPE_EMPTY)) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    long long count;
    if ((NexCacheModule_StringToLongLong(argv[3], &count) != NEXCACHEMODULE_OK) || (count < 0)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");
    }

    while (count-- > 0) {
        NexCacheModuleString *ele;

        ele = NexCacheModule_ListPop(srckey, NEXCACHEMODULE_LIST_TAIL);
        if (ele == NULL) break;
        NexCacheModule_ListPush(dstkey, NEXCACHEMODULE_LIST_HEAD, ele);
    }

    size_t len = NexCacheModule_ValueLength(srckey);
    NexCacheModule_ReplyWithLongLong(ctx, len);
    return NEXCACHEMODULE_OK;
}

/* HELLO.RAND.ARRAY <count>
 * Shows how to generate arrays as commands replies.
 * It just outputs <count> random numbers. */
int HelloRandArray_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);
    long long count;
    if (NexCacheModule_StringToLongLong(argv[1], &count) != NEXCACHEMODULE_OK || count < 0)
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");

    /* To reply with an array, we call NexCacheModule_ReplyWithArray() followed
     * by other "count" calls to other reply functions in order to generate
     * the elements of the array. */
    NexCacheModule_ReplyWithArray(ctx, count);
    while (count--) NexCacheModule_ReplyWithLongLong(ctx, rand());
    return NEXCACHEMODULE_OK;
}

/* This is a simple command to test replication. Because of the "!" modified
 * in the NexCacheModule_Call() call, the two INCRs get replicated.
 * Also note how the ECHO is replicated in an unexpected position (check
 * comments the function implementation). */
int HelloRepl1_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_AutoMemory(ctx);

    /* This will be replicated *after* the two INCR statements, since
     * the Call() replication has precedence, so the actual replication
     * stream will be:
     *
     * MULTI
     * INCR foo
     * INCR bar
     * ECHO c foo
     * EXEC
     */
    NexCacheModule_Replicate(ctx, "ECHO", "c", "foo");

    /* Using the "!" modifier we replicate the command if it
     * modified the dataset in some way. */
    NexCacheModule_Call(ctx, "INCR", "c!", "foo");
    NexCacheModule_Call(ctx, "INCR", "c!", "bar");

    NexCacheModule_ReplyWithLongLong(ctx, 0);

    return NEXCACHEMODULE_OK;
}

/* Another command to show replication. In this case, we call
 * NexCacheModule_ReplicateVerbatim() to mean we want just the command to be
 * propagated to replicas / AOF exactly as it was called by the user.
 *
 * This command also shows how to work with string objects.
 * It takes a list, and increments all the elements (that must have
 * a numerical value) by 1, returning the sum of all the elements
 * as reply.
 *
 * Usage: HELLO.REPL2 <list-key> */
int HelloRepl2_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);

    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_LIST)
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);

    size_t listlen = NexCacheModule_ValueLength(key);
    long long sum = 0;

    /* Rotate and increment. */
    while (listlen--) {
        NexCacheModuleString *ele = NexCacheModule_ListPop(key, NEXCACHEMODULE_LIST_TAIL);
        long long val;
        if (NexCacheModule_StringToLongLong(ele, &val) != NEXCACHEMODULE_OK) val = 0;
        val++;
        sum += val;
        NexCacheModuleString *newele = NexCacheModule_CreateStringFromLongLong(ctx, val);
        NexCacheModule_ListPush(key, NEXCACHEMODULE_LIST_HEAD, newele);
    }
    NexCacheModule_ReplyWithLongLong(ctx, sum);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* This is an example of strings DMA access. Given a key containing a string
 * it toggles the case of each character from lower to upper case or the
 * other way around.
 *
 * No automatic memory management is used in this example (for the sake
 * of variety).
 *
 * HELLO.TOGGLE.CASE key */
int HelloToggleCase_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);

    int keytype = NexCacheModule_KeyType(key);
    if (keytype != NEXCACHEMODULE_KEYTYPE_STRING && keytype != NEXCACHEMODULE_KEYTYPE_EMPTY) {
        NexCacheModule_CloseKey(key);
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    if (keytype == NEXCACHEMODULE_KEYTYPE_STRING) {
        size_t len, j;
        char *s = NexCacheModule_StringDMA(key, &len, NEXCACHEMODULE_WRITE);
        for (j = 0; j < len; j++) {
            if (isupper(s[j])) {
                s[j] = tolower(s[j]);
            } else {
                s[j] = toupper(s[j]);
            }
        }
    }

    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* HELLO.MORE.EXPIRE key milliseconds.
 *
 * If the key has already an associated TTL, extends it by "milliseconds"
 * milliseconds. Otherwise no operation is performed. */
int HelloMoreExpire_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */
    if (argc != 3) return NexCacheModule_WrongArity(ctx);

    mstime_t addms, expire;

    if (NexCacheModule_StringToLongLong(argv[2], &addms) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid expire time");

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    expire = NexCacheModule_GetExpire(key);
    if (expire != NEXCACHEMODULE_NO_EXPIRE) {
        expire += addms;
        NexCacheModule_SetExpire(key, expire);
    }
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* HELLO.ZSUMRANGE key startscore endscore
 * Return the sum of all the scores elements between startscore and endscore.
 *
 * The computation is performed two times, one time from start to end and
 * another time backward. The two scores, returned as a two element array,
 * should match.*/
int HelloZsumRange_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    double score_start, score_end;
    if (argc != 4) return NexCacheModule_WrongArity(ctx);

    if (NexCacheModule_StringToDouble(argv[2], &score_start) != NEXCACHEMODULE_OK ||
        NexCacheModule_StringToDouble(argv[3], &score_end) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid range");
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_ZSET) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    double scoresum_a = 0;
    double scoresum_b = 0;

    NexCacheModule_ZsetFirstInScoreRange(key, score_start, score_end, 0, 0);
    while (!NexCacheModule_ZsetRangeEndReached(key)) {
        double score;
        NexCacheModuleString *ele = NexCacheModule_ZsetRangeCurrentElement(key, &score);
        NexCacheModule_FreeString(ctx, ele);
        scoresum_a += score;
        NexCacheModule_ZsetRangeNext(key);
    }
    NexCacheModule_ZsetRangeStop(key);

    NexCacheModule_ZsetLastInScoreRange(key, score_start, score_end, 0, 0);
    while (!NexCacheModule_ZsetRangeEndReached(key)) {
        double score;
        NexCacheModuleString *ele = NexCacheModule_ZsetRangeCurrentElement(key, &score);
        NexCacheModule_FreeString(ctx, ele);
        scoresum_b += score;
        NexCacheModule_ZsetRangePrev(key);
    }

    NexCacheModule_ZsetRangeStop(key);

    NexCacheModule_CloseKey(key);

    NexCacheModule_ReplyWithArray(ctx, 2);
    NexCacheModule_ReplyWithDouble(ctx, scoresum_a);
    NexCacheModule_ReplyWithDouble(ctx, scoresum_b);
    return NEXCACHEMODULE_OK;
}

/* HELLO.LEXRANGE key min_lex max_lex min_age max_age
 * This command expects a sorted set stored at key in the following form:
 * - All the elements have score 0.
 * - Elements are pairs of "<name>:<age>", for example "Anna:52".
 * The command will return all the sorted set items that are lexicographically
 * between the specified range (using the same format as ZRANGEBYLEX)
 * and having an age between min_age and max_age. */
int HelloLexRange_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 6) return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_ZSET) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    if (NexCacheModule_ZsetFirstInLexRange(key, argv[2], argv[3]) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "invalid range");
    }

    int arraylen = 0;
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    while (!NexCacheModule_ZsetRangeEndReached(key)) {
        double score;
        NexCacheModuleString *ele = NexCacheModule_ZsetRangeCurrentElement(key, &score);
        NexCacheModule_ReplyWithString(ctx, ele);
        NexCacheModule_FreeString(ctx, ele);
        NexCacheModule_ZsetRangeNext(key);
        arraylen++;
    }
    NexCacheModule_ZsetRangeStop(key);
    NexCacheModule_ReplySetArrayLength(ctx, arraylen);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* HELLO.HCOPY key srcfield dstfield
 * This is just an example command that sets the hash field dstfield to the
 * same value of srcfield. If srcfield does not exist no operation is
 * performed.
 *
 * The command returns 1 if the copy is performed (srcfield exists) otherwise
 * 0 is returned. */
int HelloHCopy_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_HASH && type != NEXCACHEMODULE_KEYTYPE_EMPTY) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Get the old field value. */
    NexCacheModuleString *oldval;
    NexCacheModule_HashGet(key, NEXCACHEMODULE_HASH_NONE, argv[2], &oldval, NULL);
    if (oldval) {
        NexCacheModule_HashSet(key, NEXCACHEMODULE_HASH_NONE, argv[3], oldval, NULL);
    }
    NexCacheModule_ReplyWithLongLong(ctx, oldval != NULL);
    return NEXCACHEMODULE_OK;
}

/* HELLO.LEFTPAD str len ch
 * This is an implementation of the infamous LEFTPAD function, that
 * was at the center of an issue with the npm modules system in March 2016.
 *
 * LEFTPAD is a good example of using a Modules API called
 * "pool allocator", that was a famous way to allocate memory in yet another
 * open source project, the Apache web server.
 *
 * The concept is very simple: there is memory that is useful to allocate
 * only in the context of serving a request, and must be freed anyway when
 * the callback implementing the command returns. So in that case the module
 * does not need to retain a reference to these allocations, it is just
 * required to free the memory before returning. When this is the case the
 * module can call NexCacheModule_PoolAlloc() instead, that works like malloc()
 * but will automatically free the memory when the module callback returns.
 *
 * Note that PoolAlloc() does not necessarily require AutoMemory to be
 * active. */
int HelloLeftPad_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */
    long long padlen;

    if (argc != 4) return NexCacheModule_WrongArity(ctx);

    if ((NexCacheModule_StringToLongLong(argv[2], &padlen) != NEXCACHEMODULE_OK) || (padlen < 0)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid padding length");
    }
    size_t strlen, chlen;
    const char *str = NexCacheModule_StringPtrLen(argv[1], &strlen);
    const char *ch = NexCacheModule_StringPtrLen(argv[3], &chlen);

    /* If the string is already larger than the target len, just return
     * the string itself. */
    if (strlen >= (size_t)padlen) return NexCacheModule_ReplyWithString(ctx, argv[1]);

    /* Padding must be a single character in this simple implementation. */
    if (chlen != 1) return NexCacheModule_ReplyWithError(ctx, "ERR padding must be a single char");

    /* Here we use our pool allocator, for our throw-away allocation. */
    padlen -= strlen;
    char *buf = NexCacheModule_PoolAlloc(ctx, padlen + strlen);
    for (long long j = 0; j < padlen; j++) buf[j] = *ch;
    memcpy(buf + padlen, str, strlen);

    NexCacheModule_ReplyWithStringBuffer(ctx, buf, padlen + strlen);
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (NexCacheModule_Init(ctx, "helloworld", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Log the list of parameters passing loading the module. */
    for (int j = 0; j < argc; j++) {
        const char *s = NexCacheModule_StringPtrLen(argv[j], NULL);
        printf("Module loaded with ARGV[%d] = %s\n", j, s);
    }

    if (NexCacheModule_CreateCommand(ctx, "hello.simple", HelloSimple_NexCacheCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.push.native", HelloPushNative_NexCacheCommand, "write deny-oom", 1, 1,
                                   1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.push.call", HelloPushCall_NexCacheCommand, "write deny-oom", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.push.call2", HelloPushCall2_NexCacheCommand, "write deny-oom", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.list.sum.len", HelloListSumLen_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.list.splice", HelloListSplice_NexCacheCommand, "write deny-oom", 1, 2,
                                   1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.list.splice.auto", HelloListSpliceAuto_NexCacheCommand, "write deny-oom",
                                   1, 2, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.rand.array", HelloRandArray_NexCacheCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.repl1", HelloRepl1_NexCacheCommand, "write", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.repl2", HelloRepl2_NexCacheCommand, "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.toggle.case", HelloToggleCase_NexCacheCommand, "write", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.more.expire", HelloMoreExpire_NexCacheCommand, "write", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.zsumrange", HelloZsumRange_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.lexrange", HelloLexRange_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.hcopy", HelloHCopy_NexCacheCommand, "write deny-oom", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hello.leftpad", HelloLeftPad_NexCacheCommand, "", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
