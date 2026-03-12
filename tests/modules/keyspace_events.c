/* This module is used to test the server keyspace events API.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2020, Meir Shpilraien <meir at nexcachelabs dot com>
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

#define _BSD_SOURCE
#define _DEFAULT_SOURCE /* For usleep */

#include "nexcachemodule.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

ustime_t cached_time = 0;

/** stores all the keys on which we got 'loaded' keyspace notification **/
NexCacheModuleDict *loaded_event_log = NULL;
/** stores all the keys on which we got 'module' keyspace notification **/
NexCacheModuleDict *module_event_log = NULL;

/** Counts how many deleted KSN we got on keys with a prefix of "count_dels_" **/
static size_t dels = 0;

static int KeySpace_NotificationLoaded(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);

    if(strcmp(event, "loaded") == 0){
        const char* keyName = NexCacheModule_StringPtrLen(key, NULL);
        int nokey;
        NexCacheModule_DictGetC(loaded_event_log, (void*)keyName, strlen(keyName), &nokey);
        if(nokey){
            NexCacheModule_DictSetC(loaded_event_log, (void*)keyName, strlen(keyName), NexCacheModule_HoldString(ctx, key));
        }
    }

    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationGeneric(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(type);
    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);
    if (strncmp(key_str, "count_dels_", 11) == 0 && strcmp(event, "del") == 0) {
        if (NexCacheModule_GetContextFlags(ctx) & NEXCACHEMODULE_CTX_FLAGS_PRIMARY) {
            dels++;
            NexCacheModule_Replicate(ctx, "keyspace.incr_dels", "");
        }
        return NEXCACHEMODULE_OK;
    }
    if (cached_time) {
        NexCacheModule_Assert(cached_time == NexCacheModule_CachedMicroseconds());
        usleep(1);
        NexCacheModule_Assert(cached_time != NexCacheModule_Microseconds());
    }

    if (strcmp(event, "del") == 0) {
        NexCacheModuleString *copykey = NexCacheModule_CreateStringPrintf(ctx, "%s_copy", NexCacheModule_StringPtrLen(key, NULL));
        NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "DEL", "s!", copykey);
        NexCacheModule_FreeString(ctx, copykey);
        NexCacheModule_FreeCallReply(rep);

        int ctx_flags = NexCacheModule_GetContextFlags(ctx);
        if (ctx_flags & NEXCACHEMODULE_CTX_FLAGS_LUA) {
            NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "c", "lua");
            NexCacheModule_FreeCallReply(rep);
        }
        if (ctx_flags & NEXCACHEMODULE_CTX_FLAGS_MULTI) {
            NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "c", "multi");
            NexCacheModule_FreeCallReply(rep);
        }
    }

    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationExpired(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NEXCACHEMODULE_NOT_USED(key);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "c!", "testkeyspace:expired");
    NexCacheModule_FreeCallReply(rep);

    return NEXCACHEMODULE_OK;
}

/* This key miss notification handler is performing a write command inside the notification callback.
 * Notice, it is discourage and currently wrong to perform a write command inside key miss event.
 * It can cause read commands to be replicated to the replica/aof. This test is here temporary (for coverage and
 * verification that it's not crashing). */
static int KeySpace_NotificationModuleKeyMiss(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NEXCACHEMODULE_NOT_USED(key);

    int flags = NexCacheModule_GetContextFlags(ctx);
    if (!(flags & NEXCACHEMODULE_CTX_FLAGS_PRIMARY)) {
        return NEXCACHEMODULE_OK; // ignore the event on replica
    }

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "incr", "!c", "missed");
    NexCacheModule_FreeCallReply(rep);

    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationModuleString(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NexCacheModuleKey *nexcache_key = NexCacheModule_OpenKey(ctx, key, NEXCACHEMODULE_READ);

    size_t len = 0;
    /* NexCacheModule_StringDMA could change the data format and cause the old robj to be freed.
     * This code verifies that such format change will not cause any crashes.*/
    char *data = NexCacheModule_StringDMA(nexcache_key, &len, NEXCACHEMODULE_READ);
    int res = strncmp(data, "dummy", 5);
    NEXCACHEMODULE_NOT_USED(res);

    NexCacheModule_CloseKey(nexcache_key);

    return NEXCACHEMODULE_OK;
}

static void KeySpace_PostNotificationStringFreePD(void *pd) {
    NexCacheModule_FreeString(NULL, pd);
}

static void KeySpace_PostNotificationString(NexCacheModuleCtx *ctx, void *pd) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "incr", "!s", pd);
    NexCacheModule_FreeCallReply(rep);
}

static int KeySpace_NotificationModuleStringPostNotificationJob(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);

    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);

    if (strncmp(key_str, "string1_", 8) != 0) {
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString *new_key = NexCacheModule_CreateStringPrintf(NULL, "string_changed{%s}", key_str);
    NexCacheModule_AddPostNotificationJob(ctx, KeySpace_PostNotificationString, new_key, KeySpace_PostNotificationStringFreePD);
    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationModule(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);

    const char* keyName = NexCacheModule_StringPtrLen(key, NULL);
    int nokey;
    NexCacheModule_DictGetC(module_event_log, (void*)keyName, strlen(keyName), &nokey);
    if(nokey){
        NexCacheModule_DictSetC(module_event_log, (void*)keyName, strlen(keyName), NexCacheModule_HoldString(ctx, key));
    }
    return NEXCACHEMODULE_OK;
}

static int cmdNotify(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }

    NexCacheModule_NotifyKeyspaceEvent(ctx, NEXCACHEMODULE_NOTIFY_MODULE, "notify", argv[1]);
    NexCacheModule_ReplyWithNull(ctx);
    return NEXCACHEMODULE_OK;
}

static int cmdIsModuleKeyNotified(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* key  = NexCacheModule_StringPtrLen(argv[1], NULL);

    int nokey;
    NexCacheModuleString* keyStr = NexCacheModule_DictGetC(module_event_log, (void*)key, strlen(key), &nokey);

    NexCacheModule_ReplyWithArray(ctx, 2);
    NexCacheModule_ReplyWithLongLong(ctx, !nokey);
    if(nokey){
        NexCacheModule_ReplyWithNull(ctx);
    }else{
        NexCacheModule_ReplyWithString(ctx, keyStr);
    }
    return NEXCACHEMODULE_OK;
}

static int cmdIsKeyLoaded(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* key  = NexCacheModule_StringPtrLen(argv[1], NULL);

    int nokey;
    NexCacheModuleString* keyStr = NexCacheModule_DictGetC(loaded_event_log, (void*)key, strlen(key), &nokey);

    NexCacheModule_ReplyWithArray(ctx, 2);
    NexCacheModule_ReplyWithLongLong(ctx, !nokey);
    if(nokey){
        NexCacheModule_ReplyWithNull(ctx);
    }else{
        NexCacheModule_ReplyWithString(ctx, keyStr);
    }
    return NEXCACHEMODULE_OK;
}

static int cmdDelKeyCopy(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    cached_time = NexCacheModule_CachedMicroseconds();

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "DEL", "s!", argv[1]);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    cached_time = 0;
    return NEXCACHEMODULE_OK;
}

/* Call INCR and propagate using RM_Call with `!`. */
static int cmdIncrCase1(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "s!", argv[1]);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    return NEXCACHEMODULE_OK;
}

/* Call INCR and propagate using RM_Replicate. */
static int cmdIncrCase2(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "s", argv[1]);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    NexCacheModule_Replicate(ctx, "INCR", "s", argv[1]);
    return NEXCACHEMODULE_OK;
}

/* Call INCR and propagate using RM_ReplicateVerbatim. */
static int cmdIncrCase3(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "INCR", "s", argv[1]);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

static int cmdIncrDels(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    dels++;
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

static int cmdGetDels(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithLongLong(ctx, dels);
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (NexCacheModule_Init(ctx,"testkeyspace",1,NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    loaded_event_log = NexCacheModule_CreateDict(ctx);
    module_event_log = NexCacheModule_CreateDict(ctx);

    int keySpaceAll = NexCacheModule_GetKeyspaceNotificationFlagsAll();

    if (!(keySpaceAll & NEXCACHEMODULE_NOTIFY_LOADED)) {
        // NEXCACHEMODULE_NOTIFY_LOADED event are not supported we can not start
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_LOADED, KeySpace_NotificationLoaded) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_GENERIC, KeySpace_NotificationGeneric) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_EXPIRED, KeySpace_NotificationExpired) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_MODULE, KeySpace_NotificationModule) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_KEY_MISS, KeySpace_NotificationModuleKeyMiss) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING, KeySpace_NotificationModuleString) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING, KeySpace_NotificationModuleStringPostNotificationJob) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx,"keyspace.notify", cmdNotify,"",0,0,0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx,"keyspace.is_module_key_notified", cmdIsModuleKeyNotified,"",0,0,0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx,"keyspace.is_key_loaded", cmdIsKeyLoaded,"",0,0,0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "keyspace.del_key_copy", cmdDelKeyCopy,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }
    
    if (NexCacheModule_CreateCommand(ctx, "keyspace.incr_case1", cmdIncrCase1,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }
    
    if (NexCacheModule_CreateCommand(ctx, "keyspace.incr_case2", cmdIncrCase2,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }
    
    if (NexCacheModule_CreateCommand(ctx, "keyspace.incr_case3", cmdIncrCase3,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "keyspace.incr_dels", cmdIncrDels,
                                  "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "keyspace.get_dels", cmdGetDels,
                                  "readonly", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (argc == 1) {
        const char *ptr = NexCacheModule_StringPtrLen(argv[0], NULL);
        if (!strcasecmp(ptr, "noload")) {
            /* This is a hint that we return ERR at the last moment of OnLoad. */
            NexCacheModule_FreeDict(ctx, loaded_event_log);
            NexCacheModule_FreeDict(ctx, module_event_log);
            return NEXCACHEMODULE_ERR;
        }
    }

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(loaded_event_log, "^", NULL, 0);
    char* key;
    size_t keyLen;
    NexCacheModuleString* val;
    while((key = NexCacheModule_DictNextC(iter, &keyLen, (void**)&val))){
        NexCacheModule_FreeString(ctx, val);
    }
    NexCacheModule_FreeDict(ctx, loaded_event_log);
    NexCacheModule_DictIteratorStop(iter);
    loaded_event_log = NULL;

    iter = NexCacheModule_DictIteratorStartC(module_event_log, "^", NULL, 0);
    while((key = NexCacheModule_DictNextC(iter, &keyLen, (void**)&val))){
        NexCacheModule_FreeString(ctx, val);
    }
    NexCacheModule_FreeDict(ctx, module_event_log);
    NexCacheModule_DictIteratorStop(iter);
    module_event_log = NULL;

    return NEXCACHEMODULE_OK;
}
