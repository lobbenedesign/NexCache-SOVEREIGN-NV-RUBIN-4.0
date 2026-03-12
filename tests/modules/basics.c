/* Module designed to test the modules subsystem.
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

#include "nexcachemodule.h"
#include <string.h>
#include <stdlib.h>

/* --------------------------------- Helpers -------------------------------- */

/* Return true if the reply and the C null term string matches. */
int TestMatchReply(NexCacheModuleCallReply *reply, char *str) {
    NexCacheModuleString *mystr;
    mystr = NexCacheModule_CreateStringFromCallReply(reply);
    if (!mystr) return 0;
    const char *ptr = NexCacheModule_StringPtrLen(mystr,NULL);
    return strcmp(ptr,str) == 0;
}

/* ------------------------------- Test units ------------------------------- */

/* TEST.CALL -- Test Call() API. */
int TestCall(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","mylist");
    NexCacheModuleString *mystr = NexCacheModule_CreateString(ctx,"foo",3);
    NexCacheModule_Call(ctx,"RPUSH","csl","mylist",mystr,(long long)1234);
    reply = NexCacheModule_Call(ctx,"LRANGE","ccc","mylist","0","-1");
    long long items = NexCacheModule_CallReplyLength(reply);
    if (items != 2) goto fail;

    NexCacheModuleCallReply *item0, *item1;

    item0 = NexCacheModule_CallReplyArrayElement(reply,0);
    item1 = NexCacheModule_CallReplyArrayElement(reply,1);
    if (!TestMatchReply(item0,"foo")) goto fail;
    if (!TestMatchReply(item1,"1234")) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Attribute(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "attrib"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_STRING) goto fail;

    /* make sure we can not reply to resp2 client with resp3 (it might be a string but it contains attribute) */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    if (!TestMatchReply(reply,"Some real reply following the attribute")) goto fail;

    reply = NexCacheModule_CallReplyAttribute(reply);
    if (!reply || NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_ATTRIBUTE) goto fail;
    /* make sure we can not reply to resp2 client with resp3 attribute */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;
    if (NexCacheModule_CallReplyLength(reply) != 1) goto fail;

    NexCacheModuleCallReply *key, *val;
    if (NexCacheModule_CallReplyAttributeElement(reply,0,&key,&val) != NEXCACHEMODULE_OK) goto fail;
    if (!TestMatchReply(key,"key-popularity")) goto fail;
    if (NexCacheModule_CallReplyType(val) != NEXCACHEMODULE_REPLY_ARRAY) goto fail;
    if (NexCacheModule_CallReplyLength(val) != 2) goto fail;
    if (!TestMatchReply(NexCacheModule_CallReplyArrayElement(val, 0),"key:123")) goto fail;
    if (!TestMatchReply(NexCacheModule_CallReplyArrayElement(val, 1),"90")) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestGetResp(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    int flags = NexCacheModule_GetContextFlags(ctx);

    if (flags & NEXCACHEMODULE_CTX_FLAGS_RESP3) {
        NexCacheModule_ReplyWithLongLong(ctx, 3);
    } else {
        NexCacheModule_ReplyWithLongLong(ctx, 2);
    }

    return NEXCACHEMODULE_OK;
}

int TestCallRespAutoMode(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","myhash");
    NexCacheModule_Call(ctx,"HSET","ccccc","myhash", "f1", "v1", "f2", "v2");
    /* 0 stands for auto mode, we will get the reply in the same format as the client */
    reply = NexCacheModule_Call(ctx,"HGETALL","0c" ,"myhash");
    NexCacheModule_ReplyWithCallReply(ctx, reply);
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Map(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","myhash");
    NexCacheModule_Call(ctx,"HSET","ccccc","myhash", "f1", "v1", "f2", "v2");
    reply = NexCacheModule_Call(ctx,"HGETALL","3c" ,"myhash"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_MAP) goto fail;

    /* make sure we can not reply to resp2 client with resp3 map */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    long long items = NexCacheModule_CallReplyLength(reply);
    if (items != 2) goto fail;

    NexCacheModuleCallReply *key0, *key1;
    NexCacheModuleCallReply *val0, *val1;
    if (NexCacheModule_CallReplyMapElement(reply,0,&key0,&val0) != NEXCACHEMODULE_OK) goto fail;
    if (NexCacheModule_CallReplyMapElement(reply,1,&key1,&val1) != NEXCACHEMODULE_OK) goto fail;
    if (!TestMatchReply(key0,"f1")) goto fail;
    if (!TestMatchReply(key1,"f2")) goto fail;
    if (!TestMatchReply(val0,"v1")) goto fail;
    if (!TestMatchReply(val1,"v2")) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Bool(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "true"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_BOOL) goto fail;
    /* make sure we can not reply to resp2 client with resp3 bool */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    if (!NexCacheModule_CallReplyBool(reply)) goto fail;
    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "false"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_BOOL) goto fail;
    if (NexCacheModule_CallReplyBool(reply)) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Null(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "null"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_NULL) goto fail;

    /* make sure we can not reply to resp2 client with resp3 null */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallReplyWithNestedReply(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","mylist");
    NexCacheModule_Call(ctx,"RPUSH","ccl","mylist","test",(long long)1234);
    reply = NexCacheModule_Call(ctx,"LRANGE","ccc","mylist","0","-1");
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_ARRAY) goto fail;
    if (NexCacheModule_CallReplyLength(reply) < 1) goto fail;
    NexCacheModuleCallReply *nestedReply = NexCacheModule_CallReplyArrayElement(reply, 0);

    NexCacheModule_ReplyWithCallReply(ctx,nestedReply);
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallReplyWithArrayReply(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","mylist");
    NexCacheModule_Call(ctx,"RPUSH","ccl","mylist","test",(long long)1234);
    reply = NexCacheModule_Call(ctx,"LRANGE","ccc","mylist","0","-1");
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_ARRAY) goto fail;

    NexCacheModule_ReplyWithCallReply(ctx,reply);
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Double(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "double"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_DOUBLE) goto fail;

    /* make sure we can not reply to resp2 client with resp3 double*/
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    double d = NexCacheModule_CallReplyDouble(reply);
    /* we compare strings, since comparing doubles directly can fail in various architectures, e.g. 32bit */
    char got[30], expected[30];
    snprintf(got, sizeof(got), "%.17g", d);
    snprintf(expected, sizeof(expected), "%.17g", 3.141);
    if (strcmp(got, expected) != 0) goto fail;
    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3BigNumber(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "bignum"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_BIG_NUMBER) goto fail;

    /* make sure we can not reply to resp2 client with resp3 big number */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    size_t len;
    const char* big_num = NexCacheModule_CallReplyBigNumber(reply, &len);
    NexCacheModule_ReplyWithStringBuffer(ctx,big_num,len);
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Verbatim(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    reply = NexCacheModule_Call(ctx,"DEBUG","3cc" ,"PROTOCOL", "verbatim"); /* 3 stands for resp 3 reply */
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_VERBATIM_STRING) goto fail;

    /* make sure we can not reply to resp2 client with resp3 verbatim string */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    const char* format;
    size_t len;
    const char* str = NexCacheModule_CallReplyVerbatim(reply, &len, &format);
    NexCacheModuleString *s = NexCacheModule_CreateStringPrintf(ctx, "%.*s:%.*s", 3, format, (int)len, str);
    NexCacheModule_ReplyWithString(ctx,s);
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

int TestCallResp3Set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    NexCacheModule_Call(ctx,"DEL","c","myset");
    NexCacheModule_Call(ctx,"sadd","ccc","myset", "v1", "v2");
    reply = NexCacheModule_Call(ctx,"smembers","3c" ,"myset"); // N stands for resp 3 reply
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_SET) goto fail;

    /* make sure we can not reply to resp2 client with resp3 set */
    if (NexCacheModule_ReplyWithCallReply(ctx, reply) != NEXCACHEMODULE_ERR) goto fail;

    long long items = NexCacheModule_CallReplyLength(reply);
    if (items != 2) goto fail;

    NexCacheModuleCallReply *val0, *val1;

    val0 = NexCacheModule_CallReplySetElement(reply,0);
    val1 = NexCacheModule_CallReplySetElement(reply,1);

    /*
     * The order of elements on sets are not promised so we just
     * veridy that the reply matches one of the elements.
     */
    if (!TestMatchReply(val0,"v1") && !TestMatchReply(val0,"v2")) goto fail;
    if (!TestMatchReply(val1,"v1") && !TestMatchReply(val1,"v2")) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"OK");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,"ERR");
    return NEXCACHEMODULE_OK;
}

/* TEST.STRING.APPEND -- Test appending to an existing string object. */
int TestStringAppend(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleString *s = NexCacheModule_CreateString(ctx,"foo",3);
    NexCacheModule_StringAppendBuffer(ctx,s,"bar",3);
    NexCacheModule_ReplyWithString(ctx,s);
    NexCacheModule_FreeString(ctx,s);
    return NEXCACHEMODULE_OK;
}

/* TEST.STRING.APPEND.AM -- Test append with retain when auto memory is on. */
int TestStringAppendAM(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleString *s = NexCacheModule_CreateString(ctx,"foo",3);
    NexCacheModule_RetainString(ctx,s);
    NexCacheModule_TrimStringAllocation(s);    /* Mostly NOP, but exercises the API function */
    NexCacheModule_StringAppendBuffer(ctx,s,"bar",3);
    NexCacheModule_ReplyWithString(ctx,s);
    NexCacheModule_FreeString(ctx,s);
    return NEXCACHEMODULE_OK;
}

/* TEST.STRING.TRIM -- Test we trim a string with free space. */
int TestTrimString(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleString *s = NexCacheModule_CreateString(ctx,"foo",3);
    char *tmp = NexCacheModule_Alloc(1024);
    NexCacheModule_StringAppendBuffer(ctx,s,tmp,1024);
    size_t string_len = NexCacheModule_MallocSizeString(s);
    NexCacheModule_TrimStringAllocation(s);
    size_t len_after_trim = NexCacheModule_MallocSizeString(s);

    /* Determine if using jemalloc memory allocator. */
    NexCacheModuleServerInfoData *info = NexCacheModule_GetServerInfo(ctx, "memory");
    const char *field = NexCacheModule_ServerInfoGetFieldC(info, "mem_allocator");
    int use_jemalloc = !strncmp(field, "jemalloc", 8);

    /* Jemalloc will reallocate `s` from 2k to 1k after NexCacheModule_TrimStringAllocation(),
     * but non-jemalloc memory allocators may keep the old size. */
    if ((use_jemalloc && len_after_trim < string_len) ||
        (!use_jemalloc && len_after_trim <= string_len))
    {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        NexCacheModule_ReplyWithError(ctx, "String was not trimmed as expected.");
    }
    NexCacheModule_FreeServerInfo(ctx, info);
    NexCacheModule_Free(tmp);
    NexCacheModule_FreeString(ctx,s);
    return NEXCACHEMODULE_OK;
}

/* TEST.STRING.PRINTF -- Test string formatting. */
int TestStringPrintf(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);
    if (argc < 3) {
        return NexCacheModule_WrongArity(ctx);
    }
    NexCacheModuleString *s = NexCacheModule_CreateStringPrintf(ctx,
        "Got %d args. argv[1]: %s, argv[2]: %s",
        argc,
        NexCacheModule_StringPtrLen(argv[1], NULL),
        NexCacheModule_StringPtrLen(argv[2], NULL)
    );

    NexCacheModule_ReplyWithString(ctx,s);

    return NEXCACHEMODULE_OK;
}

int failTest(NexCacheModuleCtx *ctx, const char *msg) {
    NexCacheModule_ReplyWithError(ctx, msg);
    return NEXCACHEMODULE_ERR;
}

int TestUnlink(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleKey *k = NexCacheModule_OpenKey(ctx, NexCacheModule_CreateStringPrintf(ctx, "unlinked"), NEXCACHEMODULE_WRITE | NEXCACHEMODULE_READ);
    if (!k) return failTest(ctx, "Could not create key");

    if (NEXCACHEMODULE_ERR == NexCacheModule_StringSet(k, NexCacheModule_CreateStringPrintf(ctx, "Foobar"))) {
        return failTest(ctx, "Could not set string value");
    }

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "EXISTS", "c", "unlinked");
    if (!rep || NexCacheModule_CallReplyInteger(rep) != 1) {
        return failTest(ctx, "Key does not exist before unlink");
    }

    if (NEXCACHEMODULE_ERR == NexCacheModule_UnlinkKey(k)) {
        return failTest(ctx, "Could not unlink key");
    }

    rep = NexCacheModule_Call(ctx, "EXISTS", "c", "unlinked");
    if (!rep || NexCacheModule_CallReplyInteger(rep) != 0) {
        return failTest(ctx, "Could not verify key to be unlinked");
    }
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

int TestNestedCallReplyArrayElement(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleString *expect_key = NexCacheModule_CreateString(ctx, "mykey", strlen("mykey"));
    NexCacheModule_SelectDb(ctx, 1);
    NexCacheModule_Call(ctx, "LPUSH", "sc", expect_key, "myvalue");

    NexCacheModuleCallReply *scan_reply = NexCacheModule_Call(ctx, "SCAN", "l", (long long)0);
    NexCacheModule_Assert(scan_reply != NULL && NexCacheModule_CallReplyType(scan_reply) == NEXCACHEMODULE_REPLY_ARRAY);
    NexCacheModule_Assert(NexCacheModule_CallReplyLength(scan_reply) == 2);

    long long scan_cursor;
    NexCacheModuleCallReply *cursor_reply = NexCacheModule_CallReplyArrayElement(scan_reply, 0);
    NexCacheModule_Assert(NexCacheModule_CallReplyType(cursor_reply) == NEXCACHEMODULE_REPLY_STRING);
    NexCacheModule_Assert(NexCacheModule_StringToLongLong(NexCacheModule_CreateStringFromCallReply(cursor_reply), &scan_cursor) == NEXCACHEMODULE_OK);
    NexCacheModule_Assert(scan_cursor == 0);

    NexCacheModuleCallReply *keys_reply = NexCacheModule_CallReplyArrayElement(scan_reply, 1);
    NexCacheModule_Assert(NexCacheModule_CallReplyType(keys_reply) == NEXCACHEMODULE_REPLY_ARRAY);
    NexCacheModule_Assert( NexCacheModule_CallReplyLength(keys_reply) == 1);
 
    NexCacheModuleCallReply *key_reply = NexCacheModule_CallReplyArrayElement(keys_reply, 0);
    NexCacheModule_Assert(NexCacheModule_CallReplyType(key_reply) == NEXCACHEMODULE_REPLY_STRING);
    NexCacheModuleString *key = NexCacheModule_CreateStringFromCallReply(key_reply);
    NexCacheModule_Assert(NexCacheModule_StringCompare(key, expect_key) == 0);

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* TEST.STRING.TRUNCATE -- Test truncating an existing string object. */
int TestStringTruncate(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_Call(ctx, "SET", "cc", "foo", "abcde");
    NexCacheModuleKey *k = NexCacheModule_OpenKey(ctx, NexCacheModule_CreateStringPrintf(ctx, "foo"), NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    if (!k) return failTest(ctx, "Could not create key");

    size_t len = 0;
    char* s;

    /* expand from 5 to 8 and check null pad */
    if (NEXCACHEMODULE_ERR == NexCacheModule_StringTruncate(k, 8)) {
        return failTest(ctx, "Could not truncate string value (8)");
    }
    s = NexCacheModule_StringDMA(k, &len, NEXCACHEMODULE_READ);
    if (!s) {
        return failTest(ctx, "Failed to read truncated string (8)");
    } else if (len != 8) {
        return failTest(ctx, "Failed to expand string value (8)");
    } else if (0 != strncmp(s, "abcde\0\0\0", 8)) {
        return failTest(ctx, "Failed to null pad string value (8)");
    }

    /* shrink from 8 to 4 */
    if (NEXCACHEMODULE_ERR == NexCacheModule_StringTruncate(k, 4)) {
        return failTest(ctx, "Could not truncate string value (4)");
    }
    s = NexCacheModule_StringDMA(k, &len, NEXCACHEMODULE_READ);
    if (!s) {
        return failTest(ctx, "Failed to read truncated string (4)");
    } else if (len != 4) {
        return failTest(ctx, "Failed to shrink string value (4)");
    } else if (0 != strncmp(s, "abcd", 4)) {
        return failTest(ctx, "Failed to truncate string value (4)");
    }

    /* shrink to 0 */
    if (NEXCACHEMODULE_ERR == NexCacheModule_StringTruncate(k, 0)) {
        return failTest(ctx, "Could not truncate string value (0)");
    }
    s = NexCacheModule_StringDMA(k, &len, NEXCACHEMODULE_READ);
    if (!s) {
        return failTest(ctx, "Failed to read truncated string (0)");
    } else if (len != 0) {
        return failTest(ctx, "Failed to shrink string value to (0)");
    }

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

int NotifyCallback(NexCacheModuleCtx *ctx, int type, const char *event,
                   NexCacheModuleString *key) {
  NexCacheModule_AutoMemory(ctx);
  /* Increment a counter on the notifications: for each key notified we
   * increment a counter */
  NexCacheModule_Log(ctx, "notice", "Got event type %d, event %s, key %s", type,
                  event, NexCacheModule_StringPtrLen(key, NULL));

  NexCacheModule_Call(ctx, "HINCRBY", "csc", "notifications", key, "1");
  return NEXCACHEMODULE_OK;
}

/* TEST.NOTIFICATIONS -- Test Keyspace Notifications. */
int TestNotifications(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

#define FAIL(msg, ...)                                                                       \
    {                                                                                        \
        NexCacheModule_Log(ctx, "warning", "Failed NOTIFY Test. Reason: " #msg, ##__VA_ARGS__); \
        goto err;                                                                            \
    }
    NexCacheModule_Call(ctx, "FLUSHDB", "");

    NexCacheModule_Call(ctx, "SET", "cc", "foo", "bar");
    NexCacheModule_Call(ctx, "SET", "cc", "foo", "baz");
    NexCacheModule_Call(ctx, "SADD", "cc", "bar", "x");
    NexCacheModule_Call(ctx, "SADD", "cc", "bar", "y");

    NexCacheModule_Call(ctx, "HSET", "ccc", "baz", "x", "y");
    /* LPUSH should be ignored and not increment any counters */
    NexCacheModule_Call(ctx, "LPUSH", "cc", "l", "y");
    NexCacheModule_Call(ctx, "LPUSH", "cc", "l", "y");

    /* Miss some keys intentionally so we will get a "keymiss" notification. */
    NexCacheModule_Call(ctx, "GET", "c", "nosuchkey");
    NexCacheModule_Call(ctx, "SMEMBERS", "c", "nosuchkey");

    size_t sz;
    const char *rep;
    NexCacheModuleCallReply *r = NexCacheModule_Call(ctx, "HGET", "cc", "notifications", "foo");
    if (r == NULL || NexCacheModule_CallReplyType(r) != NEXCACHEMODULE_REPLY_STRING) {
        FAIL("Wrong or no reply for foo");
    } else {
        rep = NexCacheModule_CallReplyStringPtr(r, &sz);
        if (sz != 1 || *rep != '2') {
            FAIL("Got reply '%s'. expected '2'", NexCacheModule_CallReplyStringPtr(r, NULL));
        }
    }

    r = NexCacheModule_Call(ctx, "HGET", "cc", "notifications", "bar");
    if (r == NULL || NexCacheModule_CallReplyType(r) != NEXCACHEMODULE_REPLY_STRING) {
        FAIL("Wrong or no reply for bar");
    } else {
        rep = NexCacheModule_CallReplyStringPtr(r, &sz);
        if (sz != 1 || *rep != '2') {
            FAIL("Got reply '%s'. expected '2'", rep);
        }
    }

    r = NexCacheModule_Call(ctx, "HGET", "cc", "notifications", "baz");
    if (r == NULL || NexCacheModule_CallReplyType(r) != NEXCACHEMODULE_REPLY_STRING) {
        FAIL("Wrong or no reply for baz");
    } else {
        rep = NexCacheModule_CallReplyStringPtr(r, &sz);
        if (sz != 1 || *rep != '1') {
            FAIL("Got reply '%.*s'. expected '1'", (int)sz, rep);
        }
    }
    /* For l we expect nothing since we didn't subscribe to list events */
    r = NexCacheModule_Call(ctx, "HGET", "cc", "notifications", "l");
    if (r == NULL || NexCacheModule_CallReplyType(r) != NEXCACHEMODULE_REPLY_NULL) {
        FAIL("Wrong reply for l");
    }

    r = NexCacheModule_Call(ctx, "HGET", "cc", "notifications", "nosuchkey");
    if (r == NULL || NexCacheModule_CallReplyType(r) != NEXCACHEMODULE_REPLY_STRING) {
        FAIL("Wrong or no reply for nosuchkey");
    } else {
        rep = NexCacheModule_CallReplyStringPtr(r, &sz);
        if (sz != 1 || *rep != '2') {
            FAIL("Got reply '%.*s'. expected '2'", (int)sz, rep);
        }
    }

    NexCacheModule_Call(ctx, "FLUSHDB", "");

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
err:
    NexCacheModule_Call(ctx, "FLUSHDB", "");

    return NexCacheModule_ReplyWithSimpleString(ctx, "ERR");
}

/* test.latency latency_ms */
int TestLatency(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    long long latency_ms;
    if (NexCacheModule_StringToLongLong(argv[1], &latency_ms) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid integer value");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_LatencyAddSample("test", latency_ms);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* TEST.CTXFLAGS -- Test GetContextFlags. */
int TestCtxFlags(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argc);
    NEXCACHEMODULE_NOT_USED(argv);

    NexCacheModule_AutoMemory(ctx);

    int ok = 1;
    const char *errString = NULL;
#undef FAIL
#define FAIL(msg)        \
    {                    \
        ok = 0;          \
        errString = msg; \
        goto end;        \
    }

    int flags = NexCacheModule_GetContextFlags(ctx);
    if (flags == 0) {
        FAIL("Got no flags");
    }

    if (flags & NEXCACHEMODULE_CTX_FLAGS_LUA) FAIL("Lua flag was set");
    if (flags & NEXCACHEMODULE_CTX_FLAGS_MULTI) FAIL("Multi flag was set");

    if (flags & NEXCACHEMODULE_CTX_FLAGS_AOF) FAIL("AOF Flag was set")
    /* Enable AOF to test AOF flags */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "appendonly", "yes");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_AOF)) FAIL("AOF Flag not set after config set");

    /* Disable RDB saving and test the flag. */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "save", "");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (flags & NEXCACHEMODULE_CTX_FLAGS_RDB) FAIL("RDB Flag was set");
    /* Enable RDB to test RDB flags */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "save", "900 1");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_RDB)) FAIL("RDB Flag was not set after config set");

    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_PRIMARY)) FAIL("Master flag was not set");
    if (flags & NEXCACHEMODULE_CTX_FLAGS_REPLICA) FAIL("Slave flag was set");
    if (flags & NEXCACHEMODULE_CTX_FLAGS_READONLY) FAIL("Read-only flag was set");
    if (flags & NEXCACHEMODULE_CTX_FLAGS_CLUSTER) FAIL("Cluster flag was set");

    /* Disable maxmemory and test the flag. (it is implicitly set in 32bit builds. */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "maxmemory", "0");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (flags & NEXCACHEMODULE_CTX_FLAGS_MAXMEMORY) FAIL("Maxmemory flag was set");

    /* Enable maxmemory and test the flag. */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "maxmemory", "100000000");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_MAXMEMORY))
        FAIL("Maxmemory flag was not set after config set");

    if (flags & NEXCACHEMODULE_CTX_FLAGS_EVICT) FAIL("Eviction flag was set");
    NexCacheModule_Call(ctx, "config", "ccc", "set", "maxmemory-policy", "allkeys-lru");
    flags = NexCacheModule_GetContextFlags(ctx);
    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_EVICT)) FAIL("Eviction flag was not set after config set");

end:
    /* Revert config changes */
    NexCacheModule_Call(ctx, "config", "ccc", "set", "appendonly", "no");
    NexCacheModule_Call(ctx, "config", "ccc", "set", "save", "");
    NexCacheModule_Call(ctx, "config", "ccc", "set", "maxmemory", "0");
    NexCacheModule_Call(ctx, "config", "ccc", "set", "maxmemory-policy", "noeviction");

    if (!ok) {
        NexCacheModule_Log(ctx, "warning", "Failed CTXFLAGS Test. Reason: %s", errString);
        return NexCacheModule_ReplyWithSimpleString(ctx, "ERR");
    }

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* ----------------------------- Test framework ----------------------------- */

/* Return 1 if the reply matches the specified string, otherwise log errors
 * in the server log and return 0. */
int TestAssertErrorReply(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, char *str, size_t len) {
    NexCacheModuleString *mystr, *expected;
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_ERROR) {
        return 0;
    }

    mystr = NexCacheModule_CreateStringFromCallReply(reply);
    expected = NexCacheModule_CreateString(ctx,str,len);
    if (NexCacheModule_StringCompare(mystr,expected) != 0) {
        const char *mystr_ptr = NexCacheModule_StringPtrLen(mystr,NULL);
        const char *expected_ptr = NexCacheModule_StringPtrLen(expected,NULL);
        NexCacheModule_Log(ctx,"warning",
            "Unexpected Error reply reply '%s' (instead of '%s')",
            mystr_ptr, expected_ptr);
        return 0;
    }
    return 1;
}

int TestAssertStringReply(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, char *str, size_t len) {
    NexCacheModuleString *mystr, *expected;

    if (NexCacheModule_CallReplyType(reply) == NEXCACHEMODULE_REPLY_ERROR) {
        NexCacheModule_Log(ctx,"warning","Test error reply: %s",
            NexCacheModule_CallReplyStringPtr(reply, NULL));
        return 0;
    } else if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_STRING) {
        NexCacheModule_Log(ctx,"warning","Unexpected reply type %d",
            NexCacheModule_CallReplyType(reply));
        return 0;
    }
    mystr = NexCacheModule_CreateStringFromCallReply(reply);
    expected = NexCacheModule_CreateString(ctx,str,len);
    if (NexCacheModule_StringCompare(mystr,expected) != 0) {
        const char *mystr_ptr = NexCacheModule_StringPtrLen(mystr,NULL);
        const char *expected_ptr = NexCacheModule_StringPtrLen(expected,NULL);
        NexCacheModule_Log(ctx,"warning",
            "Unexpected string reply '%s' (instead of '%s')",
            mystr_ptr, expected_ptr);
        return 0;
    }
    return 1;
}

/* Return 1 if the reply matches the specified integer, otherwise log errors
 * in the server log and return 0. */
int TestAssertIntegerReply(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, long long expected) {
    if (NexCacheModule_CallReplyType(reply) == NEXCACHEMODULE_REPLY_ERROR) {
        NexCacheModule_Log(ctx,"warning","Test error reply: %s",
            NexCacheModule_CallReplyStringPtr(reply, NULL));
        return 0;
    } else if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_INTEGER) {
        NexCacheModule_Log(ctx,"warning","Unexpected reply type %d",
            NexCacheModule_CallReplyType(reply));
        return 0;
    }
    long long val = NexCacheModule_CallReplyInteger(reply);
    if (val != expected) {
        NexCacheModule_Log(ctx,"warning",
            "Unexpected integer reply '%lld' (instead of '%lld')",
            val, expected);
        return 0;
    }
    return 1;
}

#define T(name,...) \
    do { \
        NexCacheModule_Log(ctx,"warning","Testing %s", name); \
        reply = NexCacheModule_Call(ctx,name,__VA_ARGS__); \
    } while (0)

/* TEST.BASICS -- Run all the tests.
 * Note: it is useful to run these tests from the module rather than TCL
 * since it's easier to check the reply types like that (make a distinction
 * between 0 and "0", etc. */
int TestBasics(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_AutoMemory(ctx);
    NexCacheModuleCallReply *reply;

    /* Make sure the DB is empty before to proceed. */
    T("dbsize","");
    if (!TestAssertIntegerReply(ctx,reply,0)) goto fail;

    T("ping","");
    if (!TestAssertStringReply(ctx,reply,"PONG",4)) goto fail;

    T("test.call","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callresp3map","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callresp3set","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callresp3double","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callresp3bool","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callresp3null","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callreplywithnestedreply","");
    if (!TestAssertStringReply(ctx,reply,"test",4)) goto fail;

    T("test.callreplywithbignumberreply","");
    if (!TestAssertStringReply(ctx,reply,"1234567999999999999999999999999999999",37)) goto fail;

    T("test.callreplywithverbatimstringreply","");
    if (!TestAssertStringReply(ctx,reply,"txt:This is a verbatim\nstring",29)) goto fail;

    T("test.ctxflags","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.string.append","");
    if (!TestAssertStringReply(ctx,reply,"foobar",6)) goto fail;

    T("test.string.truncate","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.unlink","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.nestedcallreplyarray","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.string.append.am","");
    if (!TestAssertStringReply(ctx,reply,"foobar",6)) goto fail;
    
    T("test.string.trim","");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.string.printf", "cc", "foo", "bar");
    if (!TestAssertStringReply(ctx,reply,"Got 3 args. argv[1]: foo, argv[2]: bar",38)) goto fail;

    T("test.notify", "");
    if (!TestAssertStringReply(ctx,reply,"OK",2)) goto fail;

    T("test.callreplywitharrayreply", "");
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_ARRAY) goto fail;
    if (NexCacheModule_CallReplyLength(reply) != 2) goto fail;
    if (!TestAssertStringReply(ctx,NexCacheModule_CallReplyArrayElement(reply, 0),"test",4)) goto fail;
    if (!TestAssertStringReply(ctx,NexCacheModule_CallReplyArrayElement(reply, 1),"1234",4)) goto fail;

    T("foo", "E");
    if (!TestAssertErrorReply(ctx,reply,"ERR unknown command 'foo', with args beginning with: ",53)) goto fail;

    T("set", "Ec", "x");
    if (!TestAssertErrorReply(ctx,reply,"ERR wrong number of arguments for 'set' command",47)) goto fail;

    T("shutdown", "SE");
    if (!TestAssertErrorReply(ctx,reply,"ERR command 'shutdown' is not allowed on script mode",52)) goto fail;

    T("set", "WEcc", "x", "1");
    if (!TestAssertErrorReply(ctx,reply,"ERR Write command 'set' was called while write is not allowed.",62)) goto fail;

    NexCacheModule_ReplyWithSimpleString(ctx,"ALL TESTS PASSED");
    return NEXCACHEMODULE_OK;

fail:
    NexCacheModule_ReplyWithSimpleString(ctx,
        "SOME TEST DID NOT PASS! Check server logs");
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"test",1,NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Perform RM_Call inside the NexCacheModule_OnLoad
     * to verify that it works as expected without crashing.
     * The tests will verify it on different configurations
     * options (cluster/no cluster). A simple ping command
     * is enough for this test. */
    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, "ping", "");
    if (NexCacheModule_CallReplyType(reply) != NEXCACHEMODULE_REPLY_STRING) {
        NexCacheModule_FreeCallReply(reply);
        return NEXCACHEMODULE_ERR;
    }
    size_t len;
    const char *reply_str = NexCacheModule_CallReplyStringPtr(reply, &len);
    if (len != 4) {
        NexCacheModule_FreeCallReply(reply);
        return NEXCACHEMODULE_ERR;
    }
    if (memcmp(reply_str, "PONG", 4) != 0) {
        NexCacheModule_FreeCallReply(reply);
        return NEXCACHEMODULE_ERR;
    }
    NexCacheModule_FreeCallReply(reply);

    if (NexCacheModule_CreateCommand(ctx,"test.call",
        TestCall,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3map",
        TestCallResp3Map,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3attribute",
        TestCallResp3Attribute,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3set",
        TestCallResp3Set,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3double",
        TestCallResp3Double,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3bool",
        TestCallResp3Bool,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callresp3null",
        TestCallResp3Null,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callreplywitharrayreply",
        TestCallReplyWithArrayReply,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callreplywithnestedreply",
        TestCallReplyWithNestedReply,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callreplywithbignumberreply",
        TestCallResp3BigNumber,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.callreplywithverbatimstringreply",
        TestCallResp3Verbatim,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.string.append",
        TestStringAppend,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.string.trim",
        TestTrimString,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.string.append.am",
        TestStringAppendAM,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.string.truncate",
        TestStringTruncate,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.string.printf",
        TestStringPrintf,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.ctxflags",
        TestCtxFlags,"readonly",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.unlink",
        TestUnlink,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.nestedcallreplyarray",
        TestNestedCallReplyArrayElement,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.basics",
        TestBasics,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* the following commands are used by an external test and should not be added to TestBasics */
    if (NexCacheModule_CreateCommand(ctx,"test.rmcallautomode",
        TestCallRespAutoMode,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"test.getresp",
        TestGetResp,"readonly",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_SubscribeToKeyspaceEvents(ctx,
                                            NEXCACHEMODULE_NOTIFY_HASH |
                                            NEXCACHEMODULE_NOTIFY_SET |
                                            NEXCACHEMODULE_NOTIFY_STRING |
                                            NEXCACHEMODULE_NOTIFY_KEY_MISS,
                                        NotifyCallback);
    if (NexCacheModule_CreateCommand(ctx,"test.notify",
        TestNotifications,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.latency", TestLatency, "readonly", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
