/* This module is used to test the server post keyspace jobs API.
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

/* This module allow to verify 'NexCacheModule_AddPostNotificationJob' by registering to 3
 * key space event:
 * * STRINGS - the module register to all strings notifications and set post notification job
 *             that increase a counter indicating how many times the string key was changed.
 *             In addition, it increase another counter that counts the total changes that
 *             was made on all strings keys.
 * * EXPIRED - the module register to expired event and set post notification job that that
 *             counts the total number of expired events.
 * * EVICTED - the module register to evicted event and set post notification job that that
 *             counts the total number of evicted events.
 *
 * In addition, the module register a new command, 'postnotification.async_set', that performs a set
 * command from a background thread. This allows to check the 'NexCacheModule_AddPostNotificationJob' on
 * notifications that was triggered on a background thread. */

#define _BSD_SOURCE
#define _DEFAULT_SOURCE /* For usleep */

#include "nexcachemodule.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static void KeySpace_PostNotificationStringFreePD(void *pd) {
    NexCacheModule_FreeString(NULL, pd);
}

static void KeySpace_PostNotificationReadKey(NexCacheModuleCtx *ctx, void *pd) {
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "get", "!s", pd);
    NexCacheModule_FreeCallReply(rep);
}

static void KeySpace_PostNotificationString(NexCacheModuleCtx *ctx, void *pd) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "incr", "!s", pd);
    NexCacheModule_FreeCallReply(rep);
}

static int KeySpace_NotificationExpired(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NEXCACHEMODULE_NOT_USED(key);

    NexCacheModuleString *new_key = NexCacheModule_CreateString(NULL, "expired", 7);
    int res = NexCacheModule_AddPostNotificationJob(ctx, KeySpace_PostNotificationString, new_key, KeySpace_PostNotificationStringFreePD);
    if (res == NEXCACHEMODULE_ERR) KeySpace_PostNotificationStringFreePD(new_key);
    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationEvicted(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);
    NEXCACHEMODULE_NOT_USED(key);

    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);

    if (strncmp(key_str, "evicted", 7) == 0) {
        return NEXCACHEMODULE_OK; /* do not count the evicted key */
    }

    if (strncmp(key_str, "before_evicted", 14) == 0) {
        return NEXCACHEMODULE_OK; /* do not count the before_evicted key */
    }

    NexCacheModuleString *new_key = NexCacheModule_CreateString(NULL, "evicted", 7);
    int res = NexCacheModule_AddPostNotificationJob(ctx, KeySpace_PostNotificationString, new_key, KeySpace_PostNotificationStringFreePD);
    if (res == NEXCACHEMODULE_ERR) KeySpace_PostNotificationStringFreePD(new_key);
    return NEXCACHEMODULE_OK;
}

static int KeySpace_NotificationString(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);

    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);

    if (strncmp(key_str, "string_", 7) != 0) {
        return NEXCACHEMODULE_OK;
    }

    if (strcmp(key_str, "string_total") == 0) {
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString *new_key;
    if (strncmp(key_str, "string_changed{", 15) == 0) {
        new_key = NexCacheModule_CreateString(NULL, "string_total", 12);
    } else {
        new_key = NexCacheModule_CreateStringPrintf(NULL, "string_changed{%s}", key_str);
    }

    int res = NexCacheModule_AddPostNotificationJob(ctx, KeySpace_PostNotificationString, new_key, KeySpace_PostNotificationStringFreePD);
    if (res == NEXCACHEMODULE_ERR) KeySpace_PostNotificationStringFreePD(new_key);
    return NEXCACHEMODULE_OK;
}

static int KeySpace_LazyExpireInsidePostNotificationJob(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);

    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);

    if (strncmp(key_str, "read_", 5) != 0) {
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString *new_key = NexCacheModule_CreateString(NULL, key_str + 5, strlen(key_str) - 5);;
    int res = NexCacheModule_AddPostNotificationJob(ctx, KeySpace_PostNotificationReadKey, new_key, KeySpace_PostNotificationStringFreePD);
    if (res == NEXCACHEMODULE_ERR) KeySpace_PostNotificationStringFreePD(new_key);
    return NEXCACHEMODULE_OK;
}

static int KeySpace_NestedNotification(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key){
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NEXCACHEMODULE_NOT_USED(event);

    const char *key_str = NexCacheModule_StringPtrLen(key, NULL);

    if (strncmp(key_str, "write_sync_", 11) != 0) {
        return NEXCACHEMODULE_OK;
    }

    /* This test was only meant to check NEXCACHEMODULE_OPTIONS_ALLOW_NESTED_KEYSPACE_NOTIFICATIONS.
     * In general it is wrong and discourage to perform any writes inside a notification callback.  */
    NexCacheModuleString *new_key = NexCacheModule_CreateString(NULL, key_str + 11, strlen(key_str) - 11);;
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "set", "!sc", new_key, "1");
    NexCacheModule_FreeCallReply(rep);
    NexCacheModule_FreeString(NULL, new_key);
    return NEXCACHEMODULE_OK;
}

static void *KeySpace_PostNotificationsAsyncSetInner(void *arg) {
    NexCacheModuleBlockedClient *bc = arg;
    NexCacheModuleCtx *ctx = NexCacheModule_GetThreadSafeContext(bc);
    NexCacheModule_ThreadSafeContextLock(ctx);
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "set", "!cc", "string_x", "1");
    NexCacheModule_ThreadSafeContextUnlock(ctx);
    NexCacheModule_ReplyWithCallReply(ctx, rep);
    NexCacheModule_FreeCallReply(rep);

    NexCacheModule_UnblockClient(bc, NULL);
    NexCacheModule_FreeThreadSafeContext(ctx);
    return NULL;
}

static int KeySpace_PostNotificationsAsyncSet(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1)
        return NexCacheModule_WrongArity(ctx);

    pthread_t tid;
    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClient(ctx,NULL,NULL,NULL,0);

    if (pthread_create(&tid,NULL,KeySpace_PostNotificationsAsyncSetInner,bc) != 0) {
        NexCacheModule_AbortBlock(bc);
        return NexCacheModule_ReplyWithError(ctx,"-ERR Can't start thread");
    }
    return NEXCACHEMODULE_OK;
}

typedef struct KeySpace_EventPostNotificationCtx {
    NexCacheModuleString *triggered_on;
    NexCacheModuleString *new_key;
} KeySpace_EventPostNotificationCtx;

static void KeySpace_ServerEventPostNotificationFree(void *pd) {
    KeySpace_EventPostNotificationCtx *pn_ctx = pd;
    NexCacheModule_FreeString(NULL, pn_ctx->new_key);
    NexCacheModule_FreeString(NULL, pn_ctx->triggered_on);
    NexCacheModule_Free(pn_ctx);
}

static void KeySpace_ServerEventPostNotification(NexCacheModuleCtx *ctx, void *pd) {
    NEXCACHEMODULE_NOT_USED(ctx);
    KeySpace_EventPostNotificationCtx *pn_ctx = pd;
    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, "lpush", "!ss", pn_ctx->new_key, pn_ctx->triggered_on);
    NexCacheModule_FreeCallReply(rep);
}

static void KeySpace_ServerEventCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent eid, uint64_t subevent, void *data) {
    NEXCACHEMODULE_NOT_USED(eid);
    NEXCACHEMODULE_NOT_USED(data);
    if (subevent > 3) {
        NexCacheModule_Log(ctx, "warning", "Got an unexpected subevent '%llu'", (unsigned long long)subevent);
        return;
    }
    static const char* events[] = {
            "before_deleted",
            "before_expired",
            "before_evicted",
            "before_overwritten",
    };

    const NexCacheModuleString *key_name = NexCacheModule_GetKeyNameFromModuleKey(((NexCacheModuleKeyInfo*)data)->key);
    const char *key_str = NexCacheModule_StringPtrLen(key_name, NULL);

    for (int i = 0 ; i < 4 ; ++i) {
        const char *event = events[i];
        if (strncmp(key_str, event , strlen(event)) == 0) {
            return; /* don't log any event on our tracking keys */
        }
    }

    KeySpace_EventPostNotificationCtx *pn_ctx = NexCacheModule_Alloc(sizeof(*pn_ctx));
    pn_ctx->triggered_on = NexCacheModule_HoldString(NULL, (NexCacheModuleString*)key_name);
    pn_ctx->new_key = NexCacheModule_CreateString(NULL, events[subevent], strlen(events[subevent]));
    int res = NexCacheModule_AddPostNotificationJob(ctx, KeySpace_ServerEventPostNotification, pn_ctx, KeySpace_ServerEventPostNotificationFree);
    if (res == NEXCACHEMODULE_ERR) KeySpace_ServerEventPostNotificationFree(pn_ctx);
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"postnotifications",1,NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    if (!(NexCacheModule_GetModuleOptionsAll() & NEXCACHEMODULE_OPTIONS_ALLOW_NESTED_KEYSPACE_NOTIFICATIONS)) {
        return NEXCACHEMODULE_ERR;
    }

    int with_key_events = 0;
    if (argc >= 1) {
        const char *arg = NexCacheModule_StringPtrLen(argv[0], 0);
        if (strcmp(arg, "with_key_events") == 0) {
            with_key_events = 1;
        }
    }

    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_ALLOW_NESTED_KEYSPACE_NOTIFICATIONS);

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING, KeySpace_NotificationString) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING, KeySpace_LazyExpireInsidePostNotificationJob) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING, KeySpace_NestedNotification) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_EXPIRED, KeySpace_NotificationExpired) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_EVICTED, KeySpace_NotificationEvicted) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if (with_key_events) {
        if(NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_Key, KeySpace_ServerEventCallback) != NEXCACHEMODULE_OK){
            return NEXCACHEMODULE_ERR;
        }
    }

    if (NexCacheModule_CreateCommand(ctx, "postnotification.async_set", KeySpace_PostNotificationsAsyncSet,
                                      "write", 0, 0, 0) == NEXCACHEMODULE_ERR){
        return NEXCACHEMODULE_ERR;
    }

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NEXCACHEMODULE_NOT_USED(ctx);
    return NEXCACHEMODULE_OK;
}
