/* This module is used to test blocking the client during a keyspace event. */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE /* For usleep */

#include "nexcachemodule.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define EVENT_LOG_MAX_SIZE 1024

static pthread_mutex_t event_log_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct KeyspaceEventData {
    NexCacheModuleString *key;
    NexCacheModuleString *event;
} KeyspaceEventData;

typedef struct KeyspaceEventLog {
    KeyspaceEventData *log[EVENT_LOG_MAX_SIZE];
    size_t next_index;
} KeyspaceEventLog;

KeyspaceEventLog *event_log = NULL;
int unloaded = 0;

typedef struct BackgroundThreadData {
    KeyspaceEventData *event;
    NexCacheModuleBlockedClient *bc;
} BackgroundThreadData;

static void *GenericEvent_BackgroundWork(void *arg) {
    BackgroundThreadData *data = (BackgroundThreadData *)arg;
    // Sleep for 1 second
    sleep(1);
    pthread_mutex_lock(&event_log_mutex);
    if (!unloaded && event_log->next_index < EVENT_LOG_MAX_SIZE) {
        event_log->log[event_log->next_index] = data->event;
        event_log->next_index++;
    }
    pthread_mutex_unlock(&event_log_mutex);
    if (data->bc) {
        NexCacheModule_UnblockClient(data->bc, NULL);
    }
    NexCacheModule_Free(data);
    pthread_exit(NULL);
}

static int KeySpace_NotificationGeneric(NexCacheModuleCtx *ctx, int type,
                                        const char *event,
                                        NexCacheModuleString *key) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(type);
    NexCacheModuleString *retained_key = NexCacheModule_HoldString(ctx, key);
    NexCacheModuleBlockedClient *bc =
        NexCacheModule_BlockClient(ctx, NULL, NULL, NULL, 0);
    if (bc == NULL) {
        NexCacheModule_Log(ctx, NEXCACHEMODULE_LOGLEVEL_NOTICE,
                         "Failed to block for event %s on %s!", event,
                         NexCacheModule_StringPtrLen(key, NULL));
    }
    BackgroundThreadData *data =
        NexCacheModule_Alloc(sizeof(BackgroundThreadData));
    data->bc = bc;
    KeyspaceEventData *event_data =
        NexCacheModule_Alloc(sizeof(KeyspaceEventData));
    event_data->key = retained_key;
    event_data->event = NexCacheModule_CreateString(ctx, event, strlen(event));
    data->event = event_data;
    pthread_t tid;
    pthread_create(&tid, NULL, GenericEvent_BackgroundWork, (void *)data);
    return NEXCACHEMODULE_OK;
}

static int cmdGetEvents(NexCacheModuleCtx *ctx, NexCacheModuleString **argv,
                        int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    pthread_mutex_lock(&event_log_mutex);
    NexCacheModule_ReplyWithArray(ctx, event_log->next_index);
    for (size_t i = 0; i < event_log->next_index; i++) {
        NexCacheModule_ReplyWithArray(ctx, 4);
        NexCacheModule_ReplyWithStringBuffer(ctx, "event", 5);
        NexCacheModule_ReplyWithString(ctx, event_log->log[i]->event);
        NexCacheModule_ReplyWithStringBuffer(ctx, "key", 3);
        NexCacheModule_ReplyWithString(ctx, event_log->log[i]->key);
    }
    pthread_mutex_unlock(&event_log_mutex);
    return NEXCACHEMODULE_OK;
}

static int cmdClearEvents(NexCacheModuleCtx *ctx, NexCacheModuleString **argv,
                          int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    pthread_mutex_lock(&event_log_mutex);
    for (size_t i = 0; i < event_log->next_index; i++) {
        KeyspaceEventData *data = event_log->log[i];
        NexCacheModule_FreeString(ctx, data->event);
        NexCacheModule_FreeString(ctx, data->key);
        NexCacheModule_Free(data);
    }
    event_log->next_index = 0;
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    pthread_mutex_unlock(&event_log_mutex);
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each NexCache module. It is used in order to
 * register the commands into the NexCache server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv,
                        int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "testblockingkeyspacenotif", 1,
                          NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    event_log = NexCacheModule_Alloc(sizeof(KeyspaceEventLog));
    event_log->next_index = 0;
    int keySpaceAll = NexCacheModule_GetKeyspaceNotificationFlagsAll();
    if (!(keySpaceAll & NEXCACHEMODULE_NOTIFY_LOADED)) {
        // NEXCACHEMODULE_NOTIFY_LOADED event are not supported we can not start
        return NEXCACHEMODULE_ERR;
    }
    if (NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_LOADED,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_GENERIC,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_EXPIRED,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_MODULE,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(
            ctx, NEXCACHEMODULE_NOTIFY_KEY_MISS, KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_STRING,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_HASH,
                                               KeySpace_NotificationGeneric) !=
            NEXCACHEMODULE_OK ||
        NexCacheModule_CreateCommand(ctx, "b_keyspace.events", cmdGetEvents, "",
                                   0, 0, 0) == NEXCACHEMODULE_ERR ||
        NexCacheModule_CreateCommand(ctx, "b_keyspace.clear", cmdClearEvents, "",
                                   0, 0, 0) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    pthread_mutex_lock(&event_log_mutex);
    unloaded = 1;
    for (size_t i = 0; i < event_log->next_index; i++) {
        KeyspaceEventData *data = event_log->log[i];
        NexCacheModule_FreeString(ctx, data->event);
        NexCacheModule_FreeString(ctx, data->key);
        NexCacheModule_Free(data);
    }
    NexCacheModule_Free(event_log);
    pthread_mutex_unlock(&event_log_mutex);
    return NEXCACHEMODULE_OK;
}
