/* Hellodict -- An example of modules dictionary API
 *
 * This module implements a volatile key-value store on top of the
 * dictionary exported by the modules API.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2018, Redis Ltd.
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

static NexCacheModuleDict *Keyspace;

/* HELLODICT.SET <key> <value>
 *
 * Set the specified key to the specified value. */
int cmd_SET(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModule_DictSet(Keyspace, argv[1], argv[2]);
    /* We need to keep a reference to the value stored at the key, otherwise
     * it would be freed when this callback returns. */
    NexCacheModule_RetainString(NULL, argv[2]);
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* HELLODICT.GET <key>
 *
 * Return the value of the specified key, or a null reply if the key
 * is not defined. */
int cmd_GET(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleString *val = NexCacheModule_DictGet(Keyspace, argv[1], NULL);
    if (val == NULL) {
        return NexCacheModule_ReplyWithNull(ctx);
    } else {
        return NexCacheModule_ReplyWithString(ctx, val);
    }
}

/* HELLODICT.KEYRANGE <startkey> <endkey> <count>
 *
 * Return a list of matching keys, lexicographically between startkey
 * and endkey. No more than 'count' items are emitted. */
int cmd_KEYRANGE(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4) return NexCacheModule_WrongArity(ctx);

    /* Parse the count argument. */
    long long count;
    if (NexCacheModule_StringToLongLong(argv[3], &count) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid count");
    }

    /* Seek the iterator. */
    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStart(Keyspace, ">=", argv[1]);

    /* Reply with the matching items. */
    char *key;
    size_t keylen;
    long long replylen = 0; /* Keep track of the emitted array len. */
    NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_LEN);
    while ((key = NexCacheModule_DictNextC(iter, &keylen, NULL)) != NULL) {
        if (replylen >= count) break;
        if (NexCacheModule_DictCompare(iter, "<=", argv[2]) == NEXCACHEMODULE_ERR) break;
        NexCacheModule_ReplyWithStringBuffer(ctx, key, keylen);
        replylen++;
    }
    NexCacheModule_ReplySetArrayLength(ctx, replylen);

    /* Cleanup. */
    NexCacheModule_DictIteratorStop(iter);
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "hellodict", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellodict.set", cmd_SET, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellodict.get", cmd_GET, "readonly", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellodict.keyrange", cmd_KEYRANGE, "readonly", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Create our global dictionary. Here we'll set our keys and values. */
    Keyspace = NexCacheModule_CreateDict(NULL);

    return NEXCACHEMODULE_OK;
}
