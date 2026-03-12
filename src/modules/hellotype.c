/* This file implements a new module native data type called "HELLOTYPE".
 * The data structure implemented is a very simple ordered linked list of
 * 64 bit integers, in order to have something that is real world enough, but
 * at the same time, extremely simple to understand, to show how the API
 * works, how a new data type is created, and how to write basic methods
 * for RDB loading, saving and AOF rewriting.
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
#include <stdint.h>

static NexCacheModuleType *HelloType;

/* ========================== Internal data structure  =======================
 * This is just a linked list of 64 bit integers where elements are inserted
 * in-place, so it's ordered. There is no pop/push operation but just insert
 * because it is enough to show the implementation of new data types without
 * making things complex. */

struct HelloTypeNode {
    int64_t value;
    struct HelloTypeNode *next;
};

struct HelloTypeObject {
    struct HelloTypeNode *head;
    size_t len; /* Number of elements added. */
};

struct HelloTypeObject *createHelloTypeObject(void) {
    struct HelloTypeObject *o;
    o = NexCacheModule_Alloc(sizeof(*o));
    o->head = NULL;
    o->len = 0;
    return o;
}

void HelloTypeInsert(struct HelloTypeObject *o, int64_t ele) {
    struct HelloTypeNode *next = o->head, *newnode, *prev = NULL;

    while (next && next->value < ele) {
        prev = next;
        next = next->next;
    }
    newnode = NexCacheModule_Alloc(sizeof(*newnode));
    newnode->value = ele;
    newnode->next = next;
    if (prev) {
        prev->next = newnode;
    } else {
        o->head = newnode;
    }
    o->len++;
}

void HelloTypeReleaseObject(struct HelloTypeObject *o) {
    struct HelloTypeNode *cur, *next;
    cur = o->head;
    while (cur) {
        next = cur->next;
        NexCacheModule_Free(cur);
        cur = next;
    }
    NexCacheModule_Free(o);
}

/* ========================= "hellotype" type commands ======================= */

/* HELLOTYPE.INSERT key value */
int HelloTypeInsert_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != HelloType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    long long value;
    if ((NexCacheModule_StringToLongLong(argv[2], &value) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid value: must be a signed 64 bit integer");
    }

    /* Create an empty value object if the key is currently empty. */
    struct HelloTypeObject *hto;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        hto = createHelloTypeObject();
        NexCacheModule_ModuleTypeSetValue(key, HelloType, hto);
    } else {
        hto = NexCacheModule_ModuleTypeGetValue(key);
    }

    /* Insert the new element. */
    HelloTypeInsert(hto, value);
    NexCacheModule_SignalKeyAsReady(ctx, argv[1]);

    NexCacheModule_ReplyWithLongLong(ctx, hto->len);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* HELLOTYPE.RANGE key first count */
int HelloTypeRange_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 4) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != HelloType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    long long first, count;
    if (NexCacheModule_StringToLongLong(argv[2], &first) != NEXCACHEMODULE_OK ||
        NexCacheModule_StringToLongLong(argv[3], &count) != NEXCACHEMODULE_OK || first < 0 || count < 0) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid first or count parameters");
    }

    struct HelloTypeObject *hto = NexCacheModule_ModuleTypeGetValue(key);
    struct HelloTypeNode *node = hto ? hto->head : NULL;
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    long long arraylen = 0;
    while (node && count--) {
        NexCacheModule_ReplyWithLongLong(ctx, node->value);
        arraylen++;
        node = node->next;
    }
    NexCacheModule_ReplySetArrayLength(ctx, arraylen);
    return NEXCACHEMODULE_OK;
}

/* HELLOTYPE.LEN key */
int HelloTypeLen_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 2) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != HelloType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    struct HelloTypeObject *hto = NexCacheModule_ModuleTypeGetValue(key);
    NexCacheModule_ReplyWithLongLong(ctx, hto ? hto->len : 0);
    return NEXCACHEMODULE_OK;
}

/* ====================== Example of a blocking command ==================== */

/* Reply callback for blocking command HELLOTYPE.BRANGE, this will get
 * called when the key we blocked for is ready: we need to check if we
 * can really serve the client, and reply OK or ERR accordingly. */
int HelloBlock_Reply(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleString *keyname = NexCacheModule_GetBlockedClientReadyKey(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, keyname, NEXCACHEMODULE_READ);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_MODULE || NexCacheModule_ModuleTypeGetType(key) != HelloType) {
        NexCacheModule_CloseKey(key);
        return NEXCACHEMODULE_ERR;
    }

    /* In case the key is able to serve our blocked client, let's directly
     * use our original command implementation to make this example simpler. */
    NexCacheModule_CloseKey(key);
    return HelloTypeRange_NexCacheCommand(ctx, argv, argc - 1);
}

/* Timeout callback for blocking command HELLOTYPE.BRANGE */
int HelloBlock_Timeout(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

/* Private data freeing callback for HELLOTYPE.BRANGE command. */
void HelloBlock_FreeData(NexCacheModuleCtx *ctx, void *privdata) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModule_Free(privdata);
}

/* HELLOTYPE.BRANGE key first count timeout -- This is a blocking version of
 * the RANGE operation, in order to show how to use the API
 * NexCacheModule_BlockClientOnKeys(). */
int HelloTypeBRange_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 5) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != HelloType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Parse the timeout before even trying to serve the client synchronously,
     * so that we always fail ASAP on syntax errors. */
    long long timeout;
    if (NexCacheModule_StringToLongLong(argv[4], &timeout) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid timeout parameter");
    }

    /* Can we serve the reply synchronously? */
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY) {
        return HelloTypeRange_NexCacheCommand(ctx, argv, argc - 1);
    }

    /* Otherwise let's block on the key. */
    void *privdata = NexCacheModule_Alloc(100);
    NexCacheModule_BlockClientOnKeys(ctx, HelloBlock_Reply, HelloBlock_Timeout, HelloBlock_FreeData, timeout, argv + 1, 1,
                                   privdata);
    return NEXCACHEMODULE_OK;
}

/* ========================== "hellotype" type methods ======================= */

void *HelloTypeRdbLoad(NexCacheModuleIO *rdb, int encver) {
    if (encver != 0) {
        /* NexCacheModule_Log("warning","Can't load data with version %d", encver);*/
        return NULL;
    }
    uint64_t elements = NexCacheModule_LoadUnsigned(rdb);
    struct HelloTypeObject *hto = createHelloTypeObject();
    while (elements--) {
        int64_t ele = NexCacheModule_LoadSigned(rdb);
        HelloTypeInsert(hto, ele);
    }
    return hto;
}

void HelloTypeRdbSave(NexCacheModuleIO *rdb, void *value) {
    struct HelloTypeObject *hto = value;
    struct HelloTypeNode *node = hto->head;
    NexCacheModule_SaveUnsigned(rdb, hto->len);
    while (node) {
        NexCacheModule_SaveSigned(rdb, node->value);
        node = node->next;
    }
}

void HelloTypeAofRewrite(NexCacheModuleIO *aof, NexCacheModuleString *key, void *value) {
    struct HelloTypeObject *hto = value;
    struct HelloTypeNode *node = hto->head;
    while (node) {
        NexCacheModule_EmitAOF(aof, "HELLOTYPE.INSERT", "sl", key, node->value);
        node = node->next;
    }
}

/* The goal of this function is to return the amount of memory used by
 * the HelloType value. */
size_t HelloTypeMemUsage(const void *value) {
    const struct HelloTypeObject *hto = value;
    struct HelloTypeNode *node = hto->head;
    return sizeof(*hto) + sizeof(*node) * hto->len;
}

void HelloTypeFree(void *value) {
    HelloTypeReleaseObject(value);
}

void HelloTypeDigest(NexCacheModuleDigest *md, void *value) {
    struct HelloTypeObject *hto = value;
    struct HelloTypeNode *node = hto->head;
    while (node) {
        NexCacheModule_DigestAddLongLong(md, node->value);
        node = node->next;
    }
    NexCacheModule_DigestEndSequence(md);
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "hellotype", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    NexCacheModuleTypeMethods tm = {.version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
                                  .rdb_load = HelloTypeRdbLoad,
                                  .rdb_save = HelloTypeRdbSave,
                                  .aof_rewrite = HelloTypeAofRewrite,
                                  .mem_usage = HelloTypeMemUsage,
                                  .free = HelloTypeFree,
                                  .digest = HelloTypeDigest};

    HelloType = NexCacheModule_CreateDataType(ctx, "hellotype", 0, &tm);
    if (HelloType == NULL) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellotype.insert", HelloTypeInsert_NexCacheCommand, "write deny-oom", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellotype.range", HelloTypeRange_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellotype.len", HelloTypeLen_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellotype.brange", HelloTypeBRange_NexCacheCommand, "readonly", 1, 1, 1) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
