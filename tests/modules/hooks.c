/* This module is used to test the server events hooks API.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2019, NexCache Contributors.
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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

/* We need to store events to be able to test and see what we got, and we can't
 * store them in the key-space since that would mess up rdb loading (duplicates)
 * and be lost of flushdb. */
NexCacheModuleDict *event_log = NULL;
/* stores all the keys on which we got 'removed' event */
NexCacheModuleDict *removed_event_log = NULL;
/* stores all the subevent on which we got 'removed' event */
NexCacheModuleDict *removed_subevent_type = NULL;
/* stores all the keys on which we got 'removed' event with expiry information */
NexCacheModuleDict *removed_expiry_log = NULL;

typedef struct EventElement {
    long count;
    NexCacheModuleString *last_val_string;
    long last_val_int;
} EventElement;

void LogStringEvent(NexCacheModuleCtx *ctx, const char* keyname, const char* data) {
    EventElement *event = NexCacheModule_DictGetC(event_log, (void*)keyname, strlen(keyname), NULL);
    if (!event) {
        event = NexCacheModule_Alloc(sizeof(EventElement));
        memset(event, 0, sizeof(EventElement));
        NexCacheModule_DictSetC(event_log, (void*)keyname, strlen(keyname), event);
    }
    if (event->last_val_string) NexCacheModule_FreeString(ctx, event->last_val_string);
    event->last_val_string = NexCacheModule_CreateString(ctx, data, strlen(data));
    event->count++;
}

void LogNumericEvent(NexCacheModuleCtx *ctx, const char* keyname, long data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    EventElement *event = NexCacheModule_DictGetC(event_log, (void*)keyname, strlen(keyname), NULL);
    if (!event) {
        event = NexCacheModule_Alloc(sizeof(EventElement));
        memset(event, 0, sizeof(EventElement));
        NexCacheModule_DictSetC(event_log, (void*)keyname, strlen(keyname), event);
    }
    event->last_val_int = data;
    event->count++;
}

void FreeEvent(NexCacheModuleCtx *ctx, EventElement *event) {
    if (event->last_val_string)
        NexCacheModule_FreeString(ctx, event->last_val_string);
    NexCacheModule_Free(event);
}

int cmdEventCount(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    EventElement *event = NexCacheModule_DictGet(event_log, argv[1], NULL);
    NexCacheModule_ReplyWithLongLong(ctx, event? event->count: 0);
    return NEXCACHEMODULE_OK;
}

int cmdEventLast(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 2){
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    EventElement *event = NexCacheModule_DictGet(event_log, argv[1], NULL);
    if (event && event->last_val_string)
        NexCacheModule_ReplyWithString(ctx, event->last_val_string);
    else if (event)
        NexCacheModule_ReplyWithLongLong(ctx, event->last_val_int);
    else
        NexCacheModule_ReplyWithNull(ctx);
    return NEXCACHEMODULE_OK;
}

void clearEvents(NexCacheModuleCtx *ctx)
{
    NexCacheModuleString *key;
    EventElement *event;
    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(event_log, "^", NULL, 0);
    while((key = NexCacheModule_DictNext(ctx, iter, (void**)&event)) != NULL) {
        event->count = 0;
        event->last_val_int = 0;
        if (event->last_val_string) NexCacheModule_FreeString(ctx, event->last_val_string);
        event->last_val_string = NULL;
        NexCacheModule_DictDel(event_log, key, NULL);
        NexCacheModule_Free(event);
        NexCacheModule_DictIteratorReseek(iter, ">=", key);
        NexCacheModule_FreeString(ctx, key);
    }
    NexCacheModule_DictIteratorStop(iter);
}

int cmdEventsClear(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argc);
    NEXCACHEMODULE_NOT_USED(argv);
    clearEvents(ctx);
    return NEXCACHEMODULE_OK;
}

/* Client state change callback. */
void clientChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleClientInfo *ci = data;
    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_CLIENT_CHANGE_CONNECTED) ?
        "client-connected" : "client-disconnected";
    LogNumericEvent(ctx, keyname, ci->id);
}

void flushdbCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleFlushInfo *fi = data;
    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_FLUSHDB_START) ?
        "flush-start" : "flush-end";
    LogNumericEvent(ctx, keyname, fi->dbnum);
}

void roleChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    NexCacheModuleReplicationInfo *ri = data;
    char *keyname = (sub == NEXCACHEMODULE_EVENT_REPLROLECHANGED_NOW_PRIMARY) ?
        "role-master" : "role-replica";
    LogStringEvent(ctx, keyname, ri->primary_host);
}

void replicationChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_REPLICA_CHANGE_ONLINE) ?
        "replica-online" : "replica-offline";
    LogNumericEvent(ctx, keyname, 0);
}

void rasterLinkChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_PRIMARY_LINK_UP) ?
        "masterlink-up" : "masterlink-down";
    LogNumericEvent(ctx, keyname, 0);
}

void persistenceCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    char *keyname = NULL;
    switch (sub) {
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_RDB_START: keyname = "persistence-rdb-start"; break;
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_AOF_START: keyname = "persistence-aof-start"; break;
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_AOF_START: keyname = "persistence-syncaof-start"; break;
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_RDB_START: keyname = "persistence-syncrdb-start"; break;
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_ENDED: keyname = "persistence-end"; break;
        case NEXCACHEMODULE_SUBEVENT_PERSISTENCE_FAILED: keyname = "persistence-failed"; break;
    }
    /* modifying the keyspace from the fork child is not an option, using log instead */
    NexCacheModule_Log(ctx, "warning", "module-event-%s", keyname);
    if (sub == NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_RDB_START ||
        sub == NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_AOF_START) 
    {
        LogNumericEvent(ctx, keyname, 0);
    }
}

void loadingCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);

    char *keyname = NULL;
    switch (sub) {
        case NEXCACHEMODULE_SUBEVENT_LOADING_RDB_START: keyname = "loading-rdb-start"; break;
        case NEXCACHEMODULE_SUBEVENT_LOADING_AOF_START: keyname = "loading-aof-start"; break;
        case NEXCACHEMODULE_SUBEVENT_LOADING_REPL_START: keyname = "loading-repl-start"; break;
        case NEXCACHEMODULE_SUBEVENT_LOADING_ENDED: keyname = "loading-end"; break;
        case NEXCACHEMODULE_SUBEVENT_LOADING_FAILED: keyname = "loading-failed"; break;
    }
    LogNumericEvent(ctx, keyname, 0);
}

void loadingProgressCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleLoadingProgress *ei = data;
    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_LOADING_PROGRESS_RDB) ?
        "loading-progress-rdb" : "loading-progress-aof";
    LogNumericEvent(ctx, keyname, ei->progress);
}

void shutdownCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(data);
    NEXCACHEMODULE_NOT_USED(sub);

    NexCacheModule_Log(ctx, "warning", "module-event-%s", "shutdown");
}

void cronLoopCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(sub);

    NexCacheModuleCronLoop *ei = data;
    LogNumericEvent(ctx, "cron-loop", ei->hz);
}

void moduleChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleModuleChange *ei = data;
    char *keyname = (sub == NEXCACHEMODULE_SUBEVENT_MODULE_LOADED) ?
        "module-loaded" : "module-unloaded";
    LogStringEvent(ctx, keyname, ei->module_name);
}

void swapDbCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(sub);

    NexCacheModuleSwapDbInfo *ei = data;
    LogNumericEvent(ctx, "swapdb-first", ei->dbnum_first);
    LogNumericEvent(ctx, "swapdb-second", ei->dbnum_second);
}

void configChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    if (sub != NEXCACHEMODULE_SUBEVENT_CONFIG_CHANGE) {
        return;
    }

    NexCacheModuleConfigChangeV1 *ei = data;
    LogNumericEvent(ctx, "config-change-count", ei->num_changes);
    LogStringEvent(ctx, "config-change-first", ei->config_names[0]);
}

void keyInfoCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleKeyInfoV1 *ei = data;
    NexCacheModuleKey *kp = ei->key;
    NexCacheModuleString *key = (NexCacheModuleString *) NexCacheModule_GetKeyNameFromModuleKey(kp);
    const char *keyname = NexCacheModule_StringPtrLen(key, NULL);
    NexCacheModuleString *event_keyname = NexCacheModule_CreateStringPrintf(ctx, "key-info-%s", keyname);
    LogStringEvent(ctx, NexCacheModule_StringPtrLen(event_keyname, NULL), keyname);
    NexCacheModule_FreeString(ctx, event_keyname);

    /* Despite getting a key object from the callback, we also try to re-open it
     * to make sure the callback is called before it is actually removed from the keyspace. */
    NexCacheModuleKey *kp_open = NexCacheModule_OpenKey(ctx, key, NEXCACHEMODULE_READ);
    assert(NexCacheModule_ValueLength(kp) == NexCacheModule_ValueLength(kp_open));
    NexCacheModule_CloseKey(kp_open);

    /* We also try to RM_Call a command that accesses that key, also to make sure it's still in the keyspace. */
    char *size_command = NULL;
    int key_type = NexCacheModule_KeyType(kp);
    if (key_type == NEXCACHEMODULE_KEYTYPE_STRING) {
        size_command = "STRLEN";
    } else if (key_type == NEXCACHEMODULE_KEYTYPE_LIST) {
        size_command = "LLEN";
    } else if (key_type == NEXCACHEMODULE_KEYTYPE_HASH) {
        size_command = "HLEN";
    } else if (key_type == NEXCACHEMODULE_KEYTYPE_SET) {
        size_command = "SCARD";
    } else if (key_type == NEXCACHEMODULE_KEYTYPE_ZSET) {
        size_command = "ZCARD";
    } else if (key_type == NEXCACHEMODULE_KEYTYPE_STREAM) {
        size_command = "XLEN";
    }
    if (size_command != NULL) {
        NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, size_command, "s", key);
        assert(reply != NULL);
        assert(NexCacheModule_ValueLength(kp) == (size_t) NexCacheModule_CallReplyInteger(reply));
        NexCacheModule_FreeCallReply(reply);
    }

    /* Now use the key object we got from the callback for various validations. */
    NexCacheModuleString *prev = NexCacheModule_DictGetC(removed_event_log, (void*)keyname, strlen(keyname), NULL);
    /* We keep object length */
    NexCacheModuleString *v = NexCacheModule_CreateStringPrintf(ctx, "%zd", NexCacheModule_ValueLength(kp));
    /* For string type, we keep value instead of length */
    if (NexCacheModule_KeyType(kp) == NEXCACHEMODULE_KEYTYPE_STRING) {
        NexCacheModule_FreeString(ctx, v);
        size_t len;
        /* We need to access the string value with NexCacheModule_StringDMA.
         * NexCacheModule_StringDMA may call dbUnshareStringValue to free the origin object,
         * so we also can test it. */
        char *s = NexCacheModule_StringDMA(kp, &len, NEXCACHEMODULE_READ);
        v = NexCacheModule_CreateString(ctx, s, len);
    }
    NexCacheModule_DictReplaceC(removed_event_log, (void*)keyname, strlen(keyname), v);
    if (prev != NULL) {
        NexCacheModule_FreeString(ctx, prev);
    }

    const char *subevent = "deleted";
    if (sub == NEXCACHEMODULE_SUBEVENT_KEY_EXPIRED) {
        subevent = "expired";
    } else if (sub == NEXCACHEMODULE_SUBEVENT_KEY_EVICTED) {
        subevent = "evicted";
    } else if (sub == NEXCACHEMODULE_SUBEVENT_KEY_OVERWRITTEN) {
        subevent = "overwritten";
    }
    NexCacheModule_DictReplaceC(removed_subevent_type, (void*)keyname, strlen(keyname), (void *)subevent);

    NexCacheModuleString *prevexpire = NexCacheModule_DictGetC(removed_expiry_log, (void*)keyname, strlen(keyname), NULL);
    NexCacheModuleString *expire = NexCacheModule_CreateStringPrintf(ctx, "%lld", NexCacheModule_GetAbsExpire(kp));
    NexCacheModule_DictReplaceC(removed_expiry_log, (void*)keyname, strlen(keyname), (void *)expire);
    if (prevexpire != NULL) {
        NexCacheModule_FreeString(ctx, prevexpire);
    }
}

void authAttemptCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(sub);

    NexCacheModuleAuthenticationInfo *ai = data;
    LogStringEvent(ctx, "auth-attempt", ai->username);
    if (ai->module_name) {
        LogStringEvent(ctx, "auth-attempt-module", ai->module_name);
    }
    LogNumericEvent(ctx, "auth-attempt-success", ai->result == NEXCACHEMODULE_AUTH_RESULT_GRANTED);
}

void logAtomicSlotMigrationInfo(NexCacheModuleCtx *ctx, const char *prefix, NexCacheModuleAtomicSlotMigrationInfo *asmi) {
    NexCacheModuleString *job_keyname = NexCacheModule_CreateStringPrintf(ctx, "%s-jobname", prefix);
    LogStringEvent(ctx, NexCacheModule_StringPtrLen(job_keyname, NULL), asmi->job_name);
    NexCacheModule_FreeString(ctx, job_keyname);

    NexCacheModuleString *numslotranges_keyname = NexCacheModule_CreateStringPrintf(ctx, "%s-numslotranges", prefix);
    LogNumericEvent(ctx, NexCacheModule_StringPtrLen(numslotranges_keyname, NULL), asmi->num_slot_ranges);
    NexCacheModule_FreeString(ctx, numslotranges_keyname);

    NexCacheModuleString *joined_range_str = NULL;
    for (size_t i = 0; i < asmi->num_slot_ranges; i++) {
        NexCacheModuleString *range_str = NexCacheModule_CreateStringPrintf(ctx, "%d-%d",
            asmi->slot_ranges[i].start, asmi->slot_ranges[i].end);
        if (joined_range_str) {
            NexCacheModule_StringAppendBuffer(ctx, range_str, " ", 1);
            size_t range_str_len;
            const char *range_buf = NexCacheModule_StringPtrLen(range_str, &range_str_len);
            NexCacheModule_StringAppendBuffer(ctx, joined_range_str, range_buf, range_str_len);
            NexCacheModule_FreeString(ctx, range_str);
        } else {
            joined_range_str = range_str;
        }
    }
    if (!joined_range_str) {
        joined_range_str = NexCacheModule_CreateString(ctx, "", 0);
    }
    NexCacheModuleString *slotranges_keyname = NexCacheModule_CreateStringPrintf(ctx, "%s-slotranges", prefix);
    LogStringEvent(ctx, NexCacheModule_StringPtrLen(slotranges_keyname, NULL), NexCacheModule_StringPtrLen(joined_range_str, NULL));
    NexCacheModule_FreeString(ctx, slotranges_keyname);
    NexCacheModule_FreeString(ctx, joined_range_str);
}

void atomicSlotMigrationCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data)
{
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleAtomicSlotMigrationInfo *asmi = data;
    switch (sub) {
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_STARTED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-import-start", asmi);
            break;
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_COMPLETED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-import-complete", asmi);
            break;
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_ABORTED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-import-abort", asmi);
            break;
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_STARTED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-export-start", asmi);
            break;
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_COMPLETED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-export-complete", asmi);
            break;
        case NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_ABORTED:
            logAtomicSlotMigrationInfo(ctx, "atomic-slot-migration-export-abort", asmi);
            break;
    }
}

static int cmdIsKeyRemoved(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char *key  = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleString *value = NexCacheModule_DictGetC(removed_event_log, (void*)key, strlen(key), NULL);

    if (value == NULL) {
        return NexCacheModule_ReplyWithError(ctx, "ERR Key was not removed");
    }

    const char *subevent = NexCacheModule_DictGetC(removed_subevent_type, (void*)key, strlen(key), NULL);
    NexCacheModule_ReplyWithArray(ctx, 2);
    NexCacheModule_ReplyWithString(ctx, value);
    NexCacheModule_ReplyWithSimpleString(ctx, subevent);

    return NEXCACHEMODULE_OK;
}

static int cmdKeyExpiry(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* key  = NexCacheModule_StringPtrLen(argv[1], NULL);
    NexCacheModuleString *expire = NexCacheModule_DictGetC(removed_expiry_log, (void*)key, strlen(key), NULL);
    if (expire == NULL) {
        return NexCacheModule_ReplyWithError(ctx, "ERR Key was not removed");
    }
    NexCacheModule_ReplyWithString(ctx, expire);
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
#define VerifySubEventSupported(e, s) \
    if (!NexCacheModule_IsSubEventSupported(e, s)) { \
        return NEXCACHEMODULE_ERR; \
    }

    if (NexCacheModule_Init(ctx,"testhook",1,NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* Example on how to check if a server sub event is supported */
    if (!NexCacheModule_IsSubEventSupported(NexCacheModuleEvent_ReplicationRoleChanged, NEXCACHEMODULE_EVENT_REPLROLECHANGED_NOW_PRIMARY)) {
        return NEXCACHEMODULE_ERR;
    }

    /* replication related hooks */
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_ReplicationRoleChanged, roleChangeCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_ReplicaChange, replicationChangeCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_PrimaryLinkChange, rasterLinkChangeCallback);

    /* persistence related hooks */
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_Persistence, persistenceCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_Loading, loadingCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_LoadingProgress, loadingProgressCallback);

    /* other hooks */
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_ClientChange, clientChangeCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_FlushDB, flushdbCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_Shutdown, shutdownCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_CronLoop, cronLoopCallback);

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_ModuleChange, moduleChangeCallback);
    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_SwapDB, swapDbCallback);

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_Config, configChangeCallback);

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_Key, keyInfoCallback);

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_AuthenticationAttempt, authAttemptCallback);

    NexCacheModule_SubscribeToServerEvent(ctx,
        NexCacheModuleEvent_AtomicSlotMigration, atomicSlotMigrationCallback);

    event_log = NexCacheModule_CreateDict(ctx);
    removed_event_log = NexCacheModule_CreateDict(ctx);
    removed_subevent_type = NexCacheModule_CreateDict(ctx);
    removed_expiry_log = NexCacheModule_CreateDict(ctx);

    if (NexCacheModule_CreateCommand(ctx,"hooks.event_count", cmdEventCount,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"hooks.event_last", cmdEventLast,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"hooks.clear", cmdEventsClear,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"hooks.is_key_removed", cmdIsKeyRemoved,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"hooks.pexpireat", cmdKeyExpiry,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (argc == 1) {
        const char *ptr = NexCacheModule_StringPtrLen(argv[0], NULL);
        if (!strcasecmp(ptr, "noload")) {
            /* This is a hint that we return ERR at the last moment of OnLoad. */
            NexCacheModule_FreeDict(ctx, event_log);
            NexCacheModule_FreeDict(ctx, removed_event_log);
            NexCacheModule_FreeDict(ctx, removed_subevent_type);
            NexCacheModule_FreeDict(ctx, removed_expiry_log);
            return NEXCACHEMODULE_ERR;
        }
    }

    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_HANDLE_ATOMIC_SLOT_MIGRATION);

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    clearEvents(ctx);
    NexCacheModule_FreeDict(ctx, event_log);
    event_log = NULL;

    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(removed_event_log, "^", NULL, 0);
    char* key;
    size_t keyLen;
    NexCacheModuleString* val;
    while((key = NexCacheModule_DictNextC(iter, &keyLen, (void**)&val))){
        NexCacheModule_FreeString(ctx, val);
    }
    NexCacheModule_FreeDict(ctx, removed_event_log);
    NexCacheModule_DictIteratorStop(iter);
    removed_event_log = NULL;

    NexCacheModule_FreeDict(ctx, removed_subevent_type);
    removed_subevent_type = NULL;

    iter = NexCacheModule_DictIteratorStartC(removed_expiry_log, "^", NULL, 0);
    while((key = NexCacheModule_DictNextC(iter, &keyLen, (void**)&val))){
        NexCacheModule_FreeString(ctx, val);
    }
    NexCacheModule_FreeDict(ctx, removed_expiry_log);
    NexCacheModule_DictIteratorStop(iter);
    removed_expiry_log = NULL;

    return NEXCACHEMODULE_OK;
}

