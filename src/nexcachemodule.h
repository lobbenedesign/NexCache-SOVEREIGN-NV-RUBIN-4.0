/*
 * nexcachemodule.h
 *
 * This header file is forked from nexcachemodule.h to reflect the new naming conventions adopted in NexCache.
 *
 * Key Changes:
 * - Symbolic names have been changed from containing NexCacheModule, NEXCACHEMODULE, etc. to NexCacheModule, NEXCACHEMODULE, etc.
 * to align with the new module naming convention. Developers must use these new symbolic names in their module
 *   implementations.
 * - Terminology has been updated to be more inclusive: "slave" is now "replica", and "master"
 *   is now "primary". These changes are part of an effort to use more accurate and inclusive language.
 *
 * When developing modules for NexCache, ensure to include "nexcachemodule.h". This header file contains
 * the updated definitions and should be used to maintain compatibility with the changes made in NexCache.
 */

#ifndef NEXCACHEMODULE_H
#define NEXCACHEMODULE_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct NexCacheModuleString NexCacheModuleString;
typedef struct NexCacheModuleKey NexCacheModuleKey;

/* -------------- Defines NOT common between core and modules ------------- */

#if defined NEXCACHEMODULE_CORE
/* Things only defined for the modules core (server), not exported to modules
 * that include this file. */

#define NexCacheModuleString robj

#endif /* defined NEXCACHEMODULE_CORE */

#if !defined NEXCACHEMODULE_CORE && !defined NEXCACHEMODULE_CORE_MODULE
/* Things defined for modules, but not for core-modules. */

typedef long long mstime_t;
typedef long long ustime_t;

#endif /* !defined NEXCACHEMODULE_CORE && !defined NEXCACHEMODULE_CORE_MODULE */

/* ---------------- Defines common between core and modules --------------- */

/* Error status return values. */
#define NEXCACHEMODULE_OK 0
#define NEXCACHEMODULE_ERR 1

/* Module Based Authentication status return values. */
#define NEXCACHEMODULE_AUTH_HANDLED 0
#define NEXCACHEMODULE_AUTH_NOT_HANDLED 1

/* API versions. */
#define NEXCACHEMODULE_APIVER_1 1

/* Version of the NexCacheModuleTypeMethods structure. Once the NexCacheModuleTypeMethods
 * structure is changed, this version number needs to be changed synchronistically. */
#define NEXCACHEMODULE_TYPE_METHOD_VERSION 5

/* API flags and constants */
#define NEXCACHEMODULE_READ (1 << 0)
#define NEXCACHEMODULE_WRITE (1 << 1)

/* NexCacheModule_OpenKey extra flags for the 'mode' argument.
 * Avoid touching the LRU/LFU of the key when opened. */
#define NEXCACHEMODULE_OPEN_KEY_NOTOUCH (1 << 16)
/* Don't trigger keyspace event on key misses. */
#define NEXCACHEMODULE_OPEN_KEY_NONOTIFY (1 << 17)
/* Don't update keyspace hits/misses counters. */
#define NEXCACHEMODULE_OPEN_KEY_NOSTATS (1 << 18)
/* Avoid deleting lazy expired keys. */
#define NEXCACHEMODULE_OPEN_KEY_NOEXPIRE (1 << 19)
/* Avoid any effects from fetching the key */
#define NEXCACHEMODULE_OPEN_KEY_NOEFFECTS (1 << 20)
/* Mask of all NEXCACHEMODULE_OPEN_KEY_* values. Any new mode should be added to this list.
 * Should not be used directly by the module, use RM_GetOpenKeyModesAll instead.
 * Located here so when we will add new modes we will not forget to update it. */
#define _NEXCACHEMODULE_OPEN_KEY_ALL                                                                            \
    NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE | NEXCACHEMODULE_OPEN_KEY_NOTOUCH | NEXCACHEMODULE_OPEN_KEY_NONOTIFY | \
        NEXCACHEMODULE_OPEN_KEY_NOSTATS | NEXCACHEMODULE_OPEN_KEY_NOEXPIRE | NEXCACHEMODULE_OPEN_KEY_NOEFFECTS

/* List push and pop */
#define NEXCACHEMODULE_LIST_HEAD 0
#define NEXCACHEMODULE_LIST_TAIL 1

/* Key types. */
#define NEXCACHEMODULE_KEYTYPE_EMPTY 0
#define NEXCACHEMODULE_KEYTYPE_STRING 1
#define NEXCACHEMODULE_KEYTYPE_LIST 2
#define NEXCACHEMODULE_KEYTYPE_HASH 3
#define NEXCACHEMODULE_KEYTYPE_SET 4
#define NEXCACHEMODULE_KEYTYPE_ZSET 5
#define NEXCACHEMODULE_KEYTYPE_MODULE 6
#define NEXCACHEMODULE_KEYTYPE_STREAM 7

/* Reply types. */
#define NEXCACHEMODULE_REPLY_UNKNOWN -1
#define NEXCACHEMODULE_REPLY_STRING 0
#define NEXCACHEMODULE_REPLY_ERROR 1
#define NEXCACHEMODULE_REPLY_INTEGER 2
#define NEXCACHEMODULE_REPLY_ARRAY 3
#define NEXCACHEMODULE_REPLY_NULL 4
#define NEXCACHEMODULE_REPLY_MAP 5
#define NEXCACHEMODULE_REPLY_SET 6
#define NEXCACHEMODULE_REPLY_BOOL 7
#define NEXCACHEMODULE_REPLY_DOUBLE 8
#define NEXCACHEMODULE_REPLY_BIG_NUMBER 9
#define NEXCACHEMODULE_REPLY_VERBATIM_STRING 10
#define NEXCACHEMODULE_REPLY_ATTRIBUTE 11
#define NEXCACHEMODULE_REPLY_PROMISE 12
#define NEXCACHEMODULE_REPLY_SIMPLE_STRING 13
#define NEXCACHEMODULE_REPLY_ARRAY_NULL 14

/* Postponed array length. */
#define NEXCACHEMODULE_POSTPONED_ARRAY_LEN -1 /* Deprecated, please use NEXCACHEMODULE_POSTPONED_LEN */
#define NEXCACHEMODULE_POSTPONED_LEN -1

/* Expire */
#define NEXCACHEMODULE_NO_EXPIRE -1

/* Sorted set API flags. */
#define NEXCACHEMODULE_ZADD_XX (1 << 0)
#define NEXCACHEMODULE_ZADD_NX (1 << 1)
#define NEXCACHEMODULE_ZADD_ADDED (1 << 2)
#define NEXCACHEMODULE_ZADD_UPDATED (1 << 3)
#define NEXCACHEMODULE_ZADD_NOP (1 << 4)
#define NEXCACHEMODULE_ZADD_GT (1 << 5)
#define NEXCACHEMODULE_ZADD_LT (1 << 6)

/* Hash API flags. */
#define NEXCACHEMODULE_HASH_NONE 0
#define NEXCACHEMODULE_HASH_NX (1 << 0)
#define NEXCACHEMODULE_HASH_XX (1 << 1)
#define NEXCACHEMODULE_HASH_CFIELDS (1 << 2)
#define NEXCACHEMODULE_HASH_EXISTS (1 << 3)
#define NEXCACHEMODULE_HASH_COUNT_ALL (1 << 4)

#define NEXCACHEMODULE_CONFIG_DEFAULT 0                /* This is the default for a module config. */
#define NEXCACHEMODULE_CONFIG_IMMUTABLE (1ULL << 0)    /* Can this value only be set at startup? */
#define NEXCACHEMODULE_CONFIG_SENSITIVE (1ULL << 1)    /* Does this value contain sensitive information */
#define NEXCACHEMODULE_CONFIG_HIDDEN (1ULL << 4)       /* This config is hidden in `config get <pattern>` (used for tests/debugging) */
#define NEXCACHEMODULE_CONFIG_PROTECTED (1ULL << 5)    /* Becomes immutable if enable-protected-configs is enabled. */
#define NEXCACHEMODULE_CONFIG_DENY_LOADING (1ULL << 6) /* This config is forbidden during loading. */

#define NEXCACHEMODULE_CONFIG_MEMORY (1ULL << 7)   /* Indicates if this value can be set as a memory value */
#define NEXCACHEMODULE_CONFIG_BITFLAGS (1ULL << 8) /* Indicates if this value can be set as a multiple enum values */

/* StreamID type. */
typedef struct NexCacheModuleStreamID {
    uint64_t ms;
    uint64_t seq;
} NexCacheModuleStreamID;

/* StreamAdd() flags. */
#define NEXCACHEMODULE_STREAM_ADD_AUTOID (1 << 0)
/* StreamIteratorStart() flags. */
#define NEXCACHEMODULE_STREAM_ITERATOR_EXCLUSIVE (1 << 0)
#define NEXCACHEMODULE_STREAM_ITERATOR_REVERSE (1 << 1)
/* StreamIteratorTrim*() flags. */
#define NEXCACHEMODULE_STREAM_TRIM_APPROX (1 << 0)

/* Context Flags: Info about the current context returned by
 * RM_GetContextFlags(). */

/* The command is running in the context of a Lua script */
#define NEXCACHEMODULE_CTX_FLAGS_LUA (1 << 0)
/* The command is running inside a NexCache transaction */
#define NEXCACHEMODULE_CTX_FLAGS_MULTI (1 << 1)
/* The instance is a primary */
#define NEXCACHEMODULE_CTX_FLAGS_PRIMARY (1 << 2)
/* The instance is a replica */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICA (1 << 3)
/* The instance is read-only (usually meaning it's a replica as well) */
#define NEXCACHEMODULE_CTX_FLAGS_READONLY (1 << 4)
/* The instance is running in cluster mode */
#define NEXCACHEMODULE_CTX_FLAGS_CLUSTER (1 << 5)
/* The instance has AOF enabled */
#define NEXCACHEMODULE_CTX_FLAGS_AOF (1 << 6)
/* The instance has RDB enabled */
#define NEXCACHEMODULE_CTX_FLAGS_RDB (1 << 7)
/* The instance has Maxmemory set */
#define NEXCACHEMODULE_CTX_FLAGS_MAXMEMORY (1 << 8)
/* Maxmemory is set and has an eviction policy that may delete keys */
#define NEXCACHEMODULE_CTX_FLAGS_EVICT (1 << 9)
/* NexCache is out of memory according to the maxmemory flag. */
#define NEXCACHEMODULE_CTX_FLAGS_OOM (1 << 10)
/* Less than 25% of memory available according to maxmemory. */
#define NEXCACHEMODULE_CTX_FLAGS_OOM_WARNING (1 << 11)
/* The command was sent over the replication link. */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICATED (1 << 12)
/* NexCache is currently loading either from AOF or RDB. */
#define NEXCACHEMODULE_CTX_FLAGS_LOADING (1 << 13)
/* The replica has no link with its primary, note that
 * there is the inverse flag as well:
 *
 *  NEXCACHEMODULE_CTX_FLAGS_REPLICA_IS_ONLINE
 *
 * The two flags are exclusive, one or the other can be set. */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICA_IS_STALE (1 << 14)
/* The replica is trying to connect with the primary.
 * (REPL_STATE_CONNECT and REPL_STATE_CONNECTING states) */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICA_IS_CONNECTING (1 << 15)
/* THe replica is receiving an RDB file from its primary. */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICA_IS_TRANSFERRING (1 << 16)
/* The replica is online, receiving updates from its primary. */
#define NEXCACHEMODULE_CTX_FLAGS_REPLICA_IS_ONLINE (1 << 17)
/* There is currently some background process active. */
#define NEXCACHEMODULE_CTX_FLAGS_ACTIVE_CHILD (1 << 18)
/* The next EXEC will fail due to dirty CAS (touched keys). */
#define NEXCACHEMODULE_CTX_FLAGS_MULTI_DIRTY (1 << 19)
/* NexCache is currently running inside background child process. */
#define NEXCACHEMODULE_CTX_FLAGS_IS_CHILD (1 << 20)
/* The current client does not allow blocking, either called from
 * within multi, lua, or from another module using RM_Call */
#define NEXCACHEMODULE_CTX_FLAGS_DENY_BLOCKING (1 << 21)
/* The current client uses RESP3 protocol */
#define NEXCACHEMODULE_CTX_FLAGS_RESP3 (1 << 22)
/* NexCache is currently async loading database for diskless replication. */
#define NEXCACHEMODULE_CTX_FLAGS_ASYNC_LOADING (1 << 23)
/* NexCache is starting. */
#define NEXCACHEMODULE_CTX_FLAGS_SERVER_STARTUP (1 << 24)
/* The current client is the slot import client */
#define NEXCACHEMODULE_CTX_FLAGS_SLOT_IMPORT_CLIENT (1 << 25)
/* The current client is the slot export client */
#define NEXCACHEMODULE_CTX_FLAGS_SLOT_EXPORT_CLIENT (1 << 26)

/* Next context flag, must be updated when adding new flags above!
This flag should not be used directly by the module.
 * Use NexCacheModule_GetContextFlagsAll instead. */
#define _NEXCACHEMODULE_CTX_FLAGS_NEXT (1 << 27)

/* Keyspace changes notification classes. Every class is associated with a
 * character for configuration purposes.
 * NOTE: These have to be in sync with NOTIFY_* in server.h */
#define NEXCACHEMODULE_NOTIFY_KEYSPACE (1 << 0)  /* K */
#define NEXCACHEMODULE_NOTIFY_KEYEVENT (1 << 1)  /* E */
#define NEXCACHEMODULE_NOTIFY_GENERIC (1 << 2)   /* g */
#define NEXCACHEMODULE_NOTIFY_STRING (1 << 3)    /* $ */
#define NEXCACHEMODULE_NOTIFY_LIST (1 << 4)      /* l */
#define NEXCACHEMODULE_NOTIFY_SET (1 << 5)       /* s */
#define NEXCACHEMODULE_NOTIFY_HASH (1 << 6)      /* h */
#define NEXCACHEMODULE_NOTIFY_ZSET (1 << 7)      /* z */
#define NEXCACHEMODULE_NOTIFY_EXPIRED (1 << 8)   /* x */
#define NEXCACHEMODULE_NOTIFY_EVICTED (1 << 9)   /* e */
#define NEXCACHEMODULE_NOTIFY_STREAM (1 << 10)   /* t */
#define NEXCACHEMODULE_NOTIFY_KEY_MISS (1 << 11) /* m (Note: This one is excluded from NEXCACHEMODULE_NOTIFY_ALL on purpose) */
#define NEXCACHEMODULE_NOTIFY_LOADED (1 << 12)   /* module only key space notification, indicate a key loaded from rdb */
#define NEXCACHEMODULE_NOTIFY_MODULE (1 << 13)   /* d, module key space notification */
#define NEXCACHEMODULE_NOTIFY_NEW (1 << 14)      /* n, new key notification */

/* Next notification flag, must be updated when adding new flags above!
This flag should not be used directly by the module.
 * Use NexCacheModule_GetKeyspaceNotificationFlagsAll instead. */
#define _NEXCACHEMODULE_NOTIFY_NEXT (1 << 15)

#define NEXCACHEMODULE_NOTIFY_ALL                                                                                        \
    (NEXCACHEMODULE_NOTIFY_GENERIC | NEXCACHEMODULE_NOTIFY_STRING | NEXCACHEMODULE_NOTIFY_LIST | NEXCACHEMODULE_NOTIFY_SET |   \
     NEXCACHEMODULE_NOTIFY_HASH | NEXCACHEMODULE_NOTIFY_ZSET | NEXCACHEMODULE_NOTIFY_EXPIRED | NEXCACHEMODULE_NOTIFY_EVICTED | \
     NEXCACHEMODULE_NOTIFY_STREAM | NEXCACHEMODULE_NOTIFY_MODULE) /* A */

/* A special pointer that we can use between the core and the module to signal
 * field deletion, and that is impossible to be a valid pointer. */
#define NEXCACHEMODULE_HASH_DELETE ((NexCacheModuleString *)(long)1)

/* Error messages. */
#define NEXCACHEMODULE_ERRORMSG_WRONGTYPE "WRONGTYPE Operation against a key holding the wrong kind of value"

#define NEXCACHEMODULE_POSITIVE_INFINITE (1.0 / 0.0)
#define NEXCACHEMODULE_NEGATIVE_INFINITE (-1.0 / 0.0)

/* Cluster API defines. */
#define NEXCACHEMODULE_NODE_ID_LEN 40
#define NEXCACHEMODULE_NODE_MYSELF (1 << 0)
#define NEXCACHEMODULE_NODE_PRIMARY (1 << 1)
#define NEXCACHEMODULE_NODE_REPLICA (1 << 2)
#define NEXCACHEMODULE_NODE_PFAIL (1 << 3)
#define NEXCACHEMODULE_NODE_FAIL (1 << 4)
#define NEXCACHEMODULE_NODE_NOFAILOVER (1 << 5)

#define NEXCACHEMODULE_CLUSTER_FLAG_NONE 0
#define NEXCACHEMODULE_CLUSTER_FLAG_NO_FAILOVER (1 << 1)
#define NEXCACHEMODULE_CLUSTER_FLAG_NO_REDIRECTION (1 << 2)

#define NEXCACHEMODULE_NOT_USED(V) ((void)V)

/* Logging level strings */
#define NEXCACHEMODULE_LOGLEVEL_DEBUG "debug"
#define NEXCACHEMODULE_LOGLEVEL_VERBOSE "verbose"
#define NEXCACHEMODULE_LOGLEVEL_NOTICE "notice"
#define NEXCACHEMODULE_LOGLEVEL_WARNING "warning"

/* Bit flags for aux_save_triggers and the aux_load and aux_save callbacks */
#define NEXCACHEMODULE_AUX_BEFORE_RDB (1 << 0)
#define NEXCACHEMODULE_AUX_AFTER_RDB (1 << 1)

/* RM_Yield flags */
#define NEXCACHEMODULE_YIELD_FLAG_NONE (1 << 0)
#define NEXCACHEMODULE_YIELD_FLAG_CLIENTS (1 << 1)

/* RM_BlockClientOnKeysWithFlags flags */
#define NEXCACHEMODULE_BLOCK_UNBLOCK_DEFAULT (0)
#define NEXCACHEMODULE_BLOCK_UNBLOCK_DELETED (1 << 0)

/* This type represents a timer handle, and is returned when a timer is
 * registered and used in order to invalidate a timer. It's just a 64 bit
 * number, because this is how each timer is represented inside the radix tree
 * of timers that are going to expire, sorted by expire time. */
typedef uint64_t NexCacheModuleTimerID;

/* CommandFilter Flags */

/* Do filter NexCacheModule_Call() commands initiated by module itself. */
#define NEXCACHEMODULE_CMDFILTER_NOSELF (1 << 0)

/* Declare that the module can handle errors with NexCacheModule_SetModuleOptions. */
#define NEXCACHEMODULE_OPTIONS_HANDLE_IO_ERRORS (1 << 0)

/* When set, NexCache will not call NexCacheModule_SignalModifiedKey(), implicitly in
 * NexCacheModule_CloseKey, and the module needs to do that when manually when keys
 * are modified from the user's perspective, to invalidate WATCH. */
#define NEXCACHEMODULE_OPTION_NO_IMPLICIT_SIGNAL_MODIFIED (1 << 1)

/* Declare that the module can handle diskless async replication with NexCacheModule_SetModuleOptions. */
#define NEXCACHEMODULE_OPTIONS_HANDLE_REPL_ASYNC_LOAD (1 << 2)

/* Declare that the module want to get nested key space notifications.
 * If enabled, the module is responsible to break endless loop. */
#define NEXCACHEMODULE_OPTIONS_ALLOW_NESTED_KEYSPACE_NOTIFICATIONS (1 << 3)

/* Skipping command validation can improve performance by reducing the overhead associated
 * with command checking, especially in high-throughput scenarios where commands
 * are already pre-validated or trusted. */
#define NEXCACHEMODULE_OPTIONS_SKIP_COMMAND_VALIDATION (1 << 4)

/* Declare that the module can handle atomic slot migration. When not set,
 * CLUSTER MIGRATESLOTS will return an error, and the CLUSTER SETSLOTS based
 * slot migration must be used. */
#define NEXCACHEMODULE_OPTIONS_HANDLE_ATOMIC_SLOT_MIGRATION (1 << 5)

/* Next option flag, must be updated when adding new module flags above!
 * This flag should not be used directly by the module.
 * Use NexCacheModule_GetModuleOptionsAll instead. */
#define _NEXCACHEMODULE_OPTIONS_FLAGS_NEXT (1 << 6)

/* Definitions for NexCacheModule_SetCommandInfo. */

typedef enum {
    NEXCACHEMODULE_ARG_TYPE_STRING,
    NEXCACHEMODULE_ARG_TYPE_INTEGER,
    NEXCACHEMODULE_ARG_TYPE_DOUBLE,
    NEXCACHEMODULE_ARG_TYPE_KEY, /* A string, but represents a keyname */
    NEXCACHEMODULE_ARG_TYPE_PATTERN,
    NEXCACHEMODULE_ARG_TYPE_UNIX_TIME,
    NEXCACHEMODULE_ARG_TYPE_PURE_TOKEN,
    NEXCACHEMODULE_ARG_TYPE_ONEOF, /* Must have sub-arguments */
    NEXCACHEMODULE_ARG_TYPE_BLOCK  /* Must have sub-arguments */
} NexCacheModuleCommandArgType;

#define NEXCACHEMODULE_CMD_ARG_NONE (0)
#define NEXCACHEMODULE_CMD_ARG_OPTIONAL (1 << 0)       /* The argument is optional (like GET in SET command) */
#define NEXCACHEMODULE_CMD_ARG_MULTIPLE (1 << 1)       /* The argument may repeat itself (like key in DEL) */
#define NEXCACHEMODULE_CMD_ARG_MULTIPLE_TOKEN (1 << 2) /* The argument may repeat itself, and so does its token (like `GET pattern` in SORT) */
#define _NEXCACHEMODULE_CMD_ARG_NEXT (1 << 3)

typedef enum {
    NEXCACHEMODULE_KSPEC_BS_INVALID = 0, /* Must be zero. An implicitly value of
                                        * zero is provided when the field is
                                        * absent in a struct literal. */
    NEXCACHEMODULE_KSPEC_BS_UNKNOWN,
    NEXCACHEMODULE_KSPEC_BS_INDEX,
    NEXCACHEMODULE_KSPEC_BS_KEYWORD
} NexCacheModuleKeySpecBeginSearchType;

typedef enum {
    NEXCACHEMODULE_KSPEC_FK_OMITTED = 0, /* Used when the field is absent in a
                                        * struct literal. Don't use this value
                                        * explicitly. */
    NEXCACHEMODULE_KSPEC_FK_UNKNOWN,
    NEXCACHEMODULE_KSPEC_FK_RANGE,
    NEXCACHEMODULE_KSPEC_FK_KEYNUM
} NexCacheModuleKeySpecFindKeysType;

/* Key-spec flags. For details, see the documentation of
 * NexCacheModule_SetCommandInfo and the key-spec flags in server.h. */
#define NEXCACHEMODULE_CMD_KEY_RO (1ULL << 0)
#define NEXCACHEMODULE_CMD_KEY_RW (1ULL << 1)
#define NEXCACHEMODULE_CMD_KEY_OW (1ULL << 2)
#define NEXCACHEMODULE_CMD_KEY_RM (1ULL << 3)
#define NEXCACHEMODULE_CMD_KEY_ACCESS (1ULL << 4)
#define NEXCACHEMODULE_CMD_KEY_UPDATE (1ULL << 5)
#define NEXCACHEMODULE_CMD_KEY_INSERT (1ULL << 6)
#define NEXCACHEMODULE_CMD_KEY_DELETE (1ULL << 7)
#define NEXCACHEMODULE_CMD_KEY_NOT_KEY (1ULL << 8)
#define NEXCACHEMODULE_CMD_KEY_INCOMPLETE (1ULL << 9)
#define NEXCACHEMODULE_CMD_KEY_VARIABLE_FLAGS (1ULL << 10)

/* Channel flags, for details see the documentation of
 * NexCacheModule_ChannelAtPosWithFlags. */
#define NEXCACHEMODULE_CMD_CHANNEL_PATTERN (1ULL << 0)
#define NEXCACHEMODULE_CMD_CHANNEL_PUBLISH (1ULL << 1)
#define NEXCACHEMODULE_CMD_CHANNEL_SUBSCRIBE (1ULL << 2)
#define NEXCACHEMODULE_CMD_CHANNEL_UNSUBSCRIBE (1ULL << 3)

typedef struct NexCacheModuleCommandArg {
    const char *name;
    NexCacheModuleCommandArgType type;
    int key_spec_index; /* If type is KEY, this is a zero-based index of
                         * the key_spec in the command. For other types,
                         * you may specify -1. */
    const char *token;  /* If type is PURE_TOKEN, this is the token. */
    const char *summary;
    const char *since;
    int flags; /* The NEXCACHEMODULE_CMD_ARG_* macros. */
    const char *deprecated_since;
    struct NexCacheModuleCommandArg *subargs;
    const char *display_text;
} NexCacheModuleCommandArg;

typedef struct {
    const char *since;
    const char *changes;
} NexCacheModuleCommandHistoryEntry;

typedef struct {
    const char *notes;
    uint64_t flags; /* NEXCACHEMODULE_CMD_KEY_* macros. */
    NexCacheModuleKeySpecBeginSearchType begin_search_type;
    union {
        struct {
            /* The index from which we start the search for keys */
            int pos;
        } index;
        struct {
            /* The keyword that indicates the beginning of key args */
            const char *keyword;
            /* An index in argv from which to start searching.
             * Can be negative, which means start search from the end, in reverse
             * (Example: -2 means to start in reverse from the penultimate arg) */
            int startfrom;
        } keyword;
    } bs;
    NexCacheModuleKeySpecFindKeysType find_keys_type;
    union {
        struct {
            /* Index of the last key relative to the result of the begin search
             * step. Can be negative, in which case it's not relative. -1
             * indicating till the last argument, -2 one before the last and so
             * on. */
            int lastkey;
            /* How many args should we skip after finding a key, in order to
             * find the next one. */
            int keystep;
            /* If lastkey is -1, we use limit to stop the search by a factor. 0
             * and 1 mean no limit. 2 means 1/2 of the remaining args, 3 means
             * 1/3, and so on. */
            int limit;
        } range;
        struct {
            /* Index of the argument containing the number of keys to come
             * relative to the result of the begin search step */
            int keynumidx;
            /* Index of the fist key. (Usually it's just after keynumidx, in
             * which case it should be set to keynumidx + 1.) */
            int firstkey;
            /* How many args should we skip after finding a key, in order to
             * find the next one, relative to the result of the begin search
             * step. */
            int keystep;
        } keynum;
    } fk;
} NexCacheModuleCommandKeySpec;

typedef struct {
    int version;
    size_t sizeof_historyentry;
    size_t sizeof_keyspec;
    size_t sizeof_arg;
} NexCacheModuleCommandInfoVersion;

static const NexCacheModuleCommandInfoVersion NexCacheModule_CurrentCommandInfoVersion = {
    .version = 1,
    .sizeof_historyentry = sizeof(NexCacheModuleCommandHistoryEntry),
    .sizeof_keyspec = sizeof(NexCacheModuleCommandKeySpec),
    .sizeof_arg = sizeof(NexCacheModuleCommandArg)};

#define NEXCACHEMODULE_COMMAND_INFO_VERSION (&NexCacheModule_CurrentCommandInfoVersion)

typedef struct {
    /* Always set version to NEXCACHEMODULE_COMMAND_INFO_VERSION */
    const NexCacheModuleCommandInfoVersion *version;
    const char *summary;                      /* Summary of the command */
    const char *complexity;                   /* Complexity description */
    const char *since;                        /* Debut module version of the command */
    NexCacheModuleCommandHistoryEntry *history; /* History */
    /* A string of space-separated tips meant for clients/proxies regarding this
     * command */
    const char *tips;
    /* Number of arguments, it is possible to use -N to say >= N */
    int arity;
    NexCacheModuleCommandKeySpec *key_specs;
    NexCacheModuleCommandArg *args;
} NexCacheModuleCommandInfo;

/* Eventloop definitions. */
#define NEXCACHEMODULE_EVENTLOOP_READABLE 1
#define NEXCACHEMODULE_EVENTLOOP_WRITABLE 2
typedef void (*NexCacheModuleEventLoopFunc)(int fd, void *user_data, int mask);
typedef void (*NexCacheModuleEventLoopOneShotFunc)(void *user_data);

/* Server events definitions.
 * Those flags should not be used directly by the module, instead
 * the module should use NexCacheModuleEvent_* variables.
 * Note: This must be synced with moduleEventVersions */
#define NEXCACHEMODULE_EVENT_REPLICATION_ROLE_CHANGED 0
#define NEXCACHEMODULE_EVENT_PERSISTENCE 1
#define NEXCACHEMODULE_EVENT_FLUSHDB 2
#define NEXCACHEMODULE_EVENT_LOADING 3
#define NEXCACHEMODULE_EVENT_CLIENT_CHANGE 4
#define NEXCACHEMODULE_EVENT_SHUTDOWN 5
#define NEXCACHEMODULE_EVENT_REPLICA_CHANGE 6
#define NEXCACHEMODULE_EVENT_PRIMARY_LINK_CHANGE 7
#define NEXCACHEMODULE_EVENT_CRON_LOOP 8
#define NEXCACHEMODULE_EVENT_MODULE_CHANGE 9
#define NEXCACHEMODULE_EVENT_LOADING_PROGRESS 10
#define NEXCACHEMODULE_EVENT_SWAPDB 11
#define NEXCACHEMODULE_EVENT_REPL_BACKUP 12 /* Not used anymore. */
#define NEXCACHEMODULE_EVENT_FORK_CHILD 13
#define NEXCACHEMODULE_EVENT_REPL_ASYNC_LOAD 14
#define NEXCACHEMODULE_EVENT_EVENTLOOP 15
#define NEXCACHEMODULE_EVENT_CONFIG 16
#define NEXCACHEMODULE_EVENT_KEY 17
#define NEXCACHEMODULE_EVENT_AUTHENTICATION_ATTEMPT 18
#define NEXCACHEMODULE_EVENT_ATOMIC_SLOT_MIGRATION 19
#define _NEXCACHEMODULE_EVENT_NEXT 20 /* Next event flag, should be updated if a new event added. */

typedef struct NexCacheModuleEvent {
    uint64_t id;      /* NEXCACHEMODULE_EVENT_... defines. */
    uint64_t dataver; /* Version of the structure we pass as 'data'. */
} NexCacheModuleEvent;

struct NexCacheModuleCtx;
struct NexCacheModuleDefragCtx;
typedef void (*NexCacheModuleEventCallback)(struct NexCacheModuleCtx *ctx,
                                          NexCacheModuleEvent eid,
                                          uint64_t subevent,
                                          void *data);

/* IMPORTANT: When adding a new version of one of below structures that contain
 * event data (NexCacheModuleFlushInfoV1 for example) we have to avoid renaming the
 * old NexCacheModuleEvent structure.
 * For example, if we want to add NexCacheModuleFlushInfoV2, the NexCacheModuleEvent
 * structures should be:
 *      NexCacheModuleEvent_FlushDB = {
 *          NEXCACHEMODULE_EVENT_FLUSHDB,
 *          1
 *      },
 *      NexCacheModuleEvent_FlushDBV2 = {
 *          NEXCACHEMODULE_EVENT_FLUSHDB,
 *          2
 *      }
 * and NOT:
 *      NexCacheModuleEvent_FlushDBV1 = {
 *          NEXCACHEMODULE_EVENT_FLUSHDB,
 *          1
 *      },
 *      NexCacheModuleEvent_FlushDB = {
 *          NEXCACHEMODULE_EVENT_FLUSHDB,
 *          2
 *      }
 * The reason for that is forward-compatibility: We want that module that
 * compiled with a new nexcachemodule.h to be able to work with a old server,
 * unless the author explicitly decided to use the newer event type.
 */
static const NexCacheModuleEvent NexCacheModuleEvent_ReplicationRoleChanged = {NEXCACHEMODULE_EVENT_REPLICATION_ROLE_CHANGED,
                                                                           1},
                               NexCacheModuleEvent_Persistence = {NEXCACHEMODULE_EVENT_PERSISTENCE, 1},
                               NexCacheModuleEvent_FlushDB = {NEXCACHEMODULE_EVENT_FLUSHDB, 1},
                               NexCacheModuleEvent_Loading = {NEXCACHEMODULE_EVENT_LOADING, 1},
                               NexCacheModuleEvent_ClientChange = {NEXCACHEMODULE_EVENT_CLIENT_CHANGE, 1},
                               NexCacheModuleEvent_Shutdown = {NEXCACHEMODULE_EVENT_SHUTDOWN, 1},
                               NexCacheModuleEvent_ReplicaChange = {NEXCACHEMODULE_EVENT_REPLICA_CHANGE, 1},
                               NexCacheModuleEvent_CronLoop = {NEXCACHEMODULE_EVENT_CRON_LOOP, 1},
                               NexCacheModuleEvent_PrimaryLinkChange = {NEXCACHEMODULE_EVENT_PRIMARY_LINK_CHANGE, 1},
                               NexCacheModuleEvent_ModuleChange = {NEXCACHEMODULE_EVENT_MODULE_CHANGE, 1},
                               NexCacheModuleEvent_LoadingProgress = {NEXCACHEMODULE_EVENT_LOADING_PROGRESS, 1},
                               NexCacheModuleEvent_SwapDB = {NEXCACHEMODULE_EVENT_SWAPDB, 1},
                               NexCacheModuleEvent_ReplAsyncLoad = {NEXCACHEMODULE_EVENT_REPL_ASYNC_LOAD, 1},
                               NexCacheModuleEvent_ForkChild = {NEXCACHEMODULE_EVENT_FORK_CHILD, 1},
                               NexCacheModuleEvent_EventLoop = {NEXCACHEMODULE_EVENT_EVENTLOOP, 1},
                               NexCacheModuleEvent_Config = {NEXCACHEMODULE_EVENT_CONFIG, 1},
                               NexCacheModuleEvent_Key = {NEXCACHEMODULE_EVENT_KEY, 1},
                               NexCacheModuleEvent_AuthenticationAttempt = {NEXCACHEMODULE_EVENT_AUTHENTICATION_ATTEMPT, 1},
                               NexCacheModuleEvent_AtomicSlotMigration = {NEXCACHEMODULE_EVENT_ATOMIC_SLOT_MIGRATION, 1};

/* Those are values that are used for the 'subevent' callback argument. */
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_RDB_START 0
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_AOF_START 1
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_RDB_START 2
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_ENDED 3
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_FAILED 4
#define NEXCACHEMODULE_SUBEVENT_PERSISTENCE_SYNC_AOF_START 5
#define _NEXCACHEMODULE_SUBEVENT_PERSISTENCE_NEXT 6

#define NEXCACHEMODULE_SUBEVENT_LOADING_RDB_START 0
#define NEXCACHEMODULE_SUBEVENT_LOADING_AOF_START 1
#define NEXCACHEMODULE_SUBEVENT_LOADING_REPL_START 2
#define NEXCACHEMODULE_SUBEVENT_LOADING_ENDED 3
#define NEXCACHEMODULE_SUBEVENT_LOADING_FAILED 4
#define _NEXCACHEMODULE_SUBEVENT_LOADING_NEXT 5

#define NEXCACHEMODULE_SUBEVENT_CLIENT_CHANGE_CONNECTED 0
#define NEXCACHEMODULE_SUBEVENT_CLIENT_CHANGE_DISCONNECTED 1
#define _NEXCACHEMODULE_SUBEVENT_CLIENT_CHANGE_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_PRIMARY_LINK_UP 0
#define NEXCACHEMODULE_SUBEVENT_PRIMARY_LINK_DOWN 1
#define _NEXCACHEMODULE_SUBEVENT_PRIMARY_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_REPLICA_CHANGE_ONLINE 0
#define NEXCACHEMODULE_SUBEVENT_REPLICA_CHANGE_OFFLINE 1
#define _NEXCACHEMODULE_SUBEVENT_REPLICA_CHANGE_NEXT 2

#define NEXCACHEMODULE_EVENT_REPLROLECHANGED_NOW_PRIMARY 0
#define NEXCACHEMODULE_EVENT_REPLROLECHANGED_NOW_REPLICA 1
#define _NEXCACHEMODULE_EVENT_REPLROLECHANGED_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_FLUSHDB_START 0
#define NEXCACHEMODULE_SUBEVENT_FLUSHDB_END 1
#define _NEXCACHEMODULE_SUBEVENT_FLUSHDB_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_MODULE_LOADED 0
#define NEXCACHEMODULE_SUBEVENT_MODULE_UNLOADED 1
#define _NEXCACHEMODULE_SUBEVENT_MODULE_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_CONFIG_CHANGE 0
#define _NEXCACHEMODULE_SUBEVENT_CONFIG_NEXT 1

#define NEXCACHEMODULE_SUBEVENT_LOADING_PROGRESS_RDB 0
#define NEXCACHEMODULE_SUBEVENT_LOADING_PROGRESS_AOF 1
#define _NEXCACHEMODULE_SUBEVENT_LOADING_PROGRESS_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_STARTED 0
#define NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_ABORTED 1
#define NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_COMPLETED 2
#define _NEXCACHEMODULE_SUBEVENT_REPL_ASYNC_LOAD_NEXT 3

#define NEXCACHEMODULE_SUBEVENT_FORK_CHILD_BORN 0
#define NEXCACHEMODULE_SUBEVENT_FORK_CHILD_DIED 1
#define _NEXCACHEMODULE_SUBEVENT_FORK_CHILD_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_EVENTLOOP_BEFORE_SLEEP 0
#define NEXCACHEMODULE_SUBEVENT_EVENTLOOP_AFTER_SLEEP 1
#define _NEXCACHEMODULE_SUBEVENT_EVENTLOOP_NEXT 2

#define NEXCACHEMODULE_SUBEVENT_KEY_DELETED 0
#define NEXCACHEMODULE_SUBEVENT_KEY_EXPIRED 1
#define NEXCACHEMODULE_SUBEVENT_KEY_EVICTED 2
#define NEXCACHEMODULE_SUBEVENT_KEY_OVERWRITTEN 3
#define _NEXCACHEMODULE_SUBEVENT_KEY_NEXT 4

#define _NEXCACHEMODULE_SUBEVENT_SHUTDOWN_NEXT 0
#define _NEXCACHEMODULE_SUBEVENT_CRON_LOOP_NEXT 0
#define _NEXCACHEMODULE_SUBEVENT_SWAPDB_NEXT 0

#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_STARTED 0
#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_STARTED 1
#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_ABORTED 2
#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_ABORTED 3
#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_IMPORT_COMPLETED 4
#define NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_EXPORT_COMPLETED 5
#define _NEXCACHEMODULE_SUBEVENT_ATOMIC_SLOT_MIGRATION_NEXT 6

/* NexCacheModuleClientInfo flags.
 * Note: flags NEXCACHEMODULE_CLIENTINFO_FLAG_PRIMARY and below were added in NexCache 9.1 */
#define NEXCACHEMODULE_CLIENTINFO_FLAG_SSL (1 << 0)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_PUBSUB (1 << 1)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_BLOCKED (1 << 2)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_TRACKING (1 << 3)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_UNIXSOCKET (1 << 4)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_MULTI (1 << 5)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_READONLY (1 << 6)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_PRIMARY (1 << 7)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_REPLICA (1 << 8)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_MONITOR (1 << 9)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_MODULE (1 << 10)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_AUTHENTICATED (1 << 11)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_EVER_AUTHENTICATED (1 << 12)
#define NEXCACHEMODULE_CLIENTINFO_FLAG_FAKE (1 << 13)

/* Here we take all the structures that the module pass to the core
 * and the other way around. Notably the list here contains the structures
 * used by the hooks API NexCacheModule_RegisterToServerEvent().
 *
 * The structures always start with a 'version' field. This is useful
 * when we want to pass a reference to the structure to the core APIs,
 * for the APIs to fill the structure. In that case, the structure 'version'
 * field is initialized before passing it to the core, so that the core is
 * able to cast the pointer to the appropriate structure version. In this
 * way we obtain ABI compatibility.
 *
 * Here we'll list all the structure versions in case they evolve over time,
 * however using a define, we'll make sure to use the last version as the
 * public name for the module to use. */

#define NEXCACHEMODULE_CLIENTINFO_VERSION 1
typedef struct NexCacheModuleClientInfo {
    uint64_t version; /* Version of this structure for ABI compat. */
    uint64_t flags;   /* NEXCACHEMODULE_CLIENTINFO_FLAG_* */
    uint64_t id;      /* Client ID. */
    char addr[46];    /* IPv4 or IPv6 address. */
    uint16_t port;    /* TCP port. */
    uint16_t db;      /* Selected DB. */
} NexCacheModuleClientInfoV1;

#define NexCacheModuleClientInfo NexCacheModuleClientInfoV1

#define NEXCACHEMODULE_CLIENTINFO_INITIALIZER_V1 {.version = 1}

#define NEXCACHEMODULE_REPLICATIONINFO_VERSION 1
typedef struct NexCacheModuleReplicationInfo {
    uint64_t version;      /* Not used since this structure is never passed
                              from the module to the core right now. Here
                              for future compatibility. */
    int primary;           /* true if primary, false if replica */
    char *primary_host;    /* primary instance hostname for NOW_REPLICA */
    int primary_port;      /* primary instance port for NOW_REPLICA */
    char *replid1;         /* Main replication ID */
    char *replid2;         /* Secondary replication ID */
    uint64_t repl1_offset; /* Main replication offset */
    uint64_t repl2_offset; /* Offset of replid2 validity */
} NexCacheModuleReplicationInfoV1;

#define NexCacheModuleReplicationInfo NexCacheModuleReplicationInfoV1

#define NEXCACHEMODULE_FLUSHINFO_VERSION 1
typedef struct NexCacheModuleFlushInfo {
    uint64_t version; /* Not used since this structure is never passed
                         from the module to the core right now. Here
                         for future compatibility. */
    int32_t sync;     /* Synchronous or threaded flush?. */
    int32_t dbnum;    /* Flushed database number, -1 for ALL. */
} NexCacheModuleFlushInfoV1;

#define NexCacheModuleFlushInfo NexCacheModuleFlushInfoV1

#define NEXCACHEMODULE_MODULE_CHANGE_VERSION 1
typedef struct NexCacheModuleModuleChange {
    uint64_t version;        /* Not used since this structure is never passed
                                from the module to the core right now. Here
                                for future compatibility. */
    const char *module_name; /* Name of module loaded or unloaded. */
    int32_t module_version;  /* Module version. */
} NexCacheModuleModuleChangeV1;

#define NexCacheModuleModuleChange NexCacheModuleModuleChangeV1

#define NEXCACHEMODULE_CONFIGCHANGE_VERSION 1
typedef struct NexCacheModuleConfigChange {
    uint64_t version;          /* Not used since this structure is never passed
                                  from the module to the core right now. Here
                                  for future compatibility. */
    uint32_t num_changes;      /* how many NexCache config options were changed */
    const char **config_names; /* the config names that were changed */
} NexCacheModuleConfigChangeV1;

#define NexCacheModuleConfigChange NexCacheModuleConfigChangeV1

#define NEXCACHEMODULE_CRON_LOOP_VERSION 1
typedef struct NexCacheModuleCronLoopInfo {
    uint64_t version; /* Not used since this structure is never passed
                         from the module to the core right now. Here
                         for future compatibility. */
    int32_t hz;       /* Approximate number of events per second. */
} NexCacheModuleCronLoopV1;

#define NexCacheModuleCronLoop NexCacheModuleCronLoopV1

#define NEXCACHEMODULE_LOADING_PROGRESS_VERSION 1
typedef struct NexCacheModuleLoadingProgressInfo {
    uint64_t version; /* Not used since this structure is never passed
                         from the module to the core right now. Here
                         for future compatibility. */
    int32_t hz;       /* Approximate number of events per second. */
    int32_t progress; /* Approximate progress between 0 and 1024, or -1
                       * if unknown. */
} NexCacheModuleLoadingProgressV1;

#define NexCacheModuleLoadingProgress NexCacheModuleLoadingProgressV1

#define NEXCACHEMODULE_SWAPDBINFO_VERSION 1
typedef struct NexCacheModuleSwapDbInfo {
    uint64_t version;     /* Not used since this structure is never passed
                             from the module to the core right now. Here
                             for future compatibility. */
    int32_t dbnum_first;  /* Swap Db first dbnum */
    int32_t dbnum_second; /* Swap Db second dbnum */
} NexCacheModuleSwapDbInfoV1;

#define NexCacheModuleSwapDbInfo NexCacheModuleSwapDbInfoV1

#define NEXCACHEMODULE_KEYINFO_VERSION 1
typedef struct NexCacheModuleKeyInfo {
    uint64_t version;     /* Not used since this structure is never passed
                             from the module to the core right now. Here
                             for future compatibility. */
    NexCacheModuleKey *key; /* Opened key. */
} NexCacheModuleKeyInfoV1;

#define NexCacheModuleKeyInfo NexCacheModuleKeyInfoV1

#define NEXCACHEMODULE_AUTHENTICATION_INFO_VERSION 1

typedef enum {
    NEXCACHEMODULE_AUTH_RESULT_GRANTED = 0, /* Authentication succeeded. */
    NEXCACHEMODULE_AUTH_RESULT_DENIED = 1,  /* Authentication failed. */
} NexCacheModuleAuthenticationResult;

typedef struct NexCacheModuleAuthenticationInfo {
    uint64_t version;                        /* Version of this structure for ABI compat. */
    uint64_t client_id;                      /* Client ID. */
    const char *username;                    /* Username used for authentication. */
    const char *module_name;                 /* Name of the module that is handling the authentication. */
    NexCacheModuleAuthenticationResult result; /* Result of the authentication */
} NexCacheModuleAuthenticationInfoV1;

#define NexCacheModuleAuthenticationInfo NexCacheModuleAuthenticationInfoV1

#define NEXCACHEMODULE_AUTHENTICATIONINFO_INITIALIZER_V1 {.version = 1}

#define NEXCACHEMODULE_ATOMICSLOTMIGRATION_INFO_VERSION 1

typedef struct NexCacheModuleSlotRange {
    int start; /* Start slot, inclusive. */
    int end;   /* End slot, inclusive. */
} NexCacheModuleSlotRange;

typedef struct NexCacheModuleAtomicSlotMigrationInfo {
    uint64_t version;                            /* Version of this structure for ABI compat. */
    char job_name[NEXCACHEMODULE_NODE_ID_LEN + 1]; /* Unique ID for the migration operation. */
    NexCacheModuleSlotRange *slot_ranges;          /* Array of slot ranges involved in the migration. */
    uint32_t num_slot_ranges;                    /* Number of slot ranges in the array. */
} NexCacheModuleAtomicSlotMigrationInfoV1;

#define NexCacheModuleAtomicSlotMigrationInfo NexCacheModuleAtomicSlotMigrationInfoV1

#define NEXCACHEMODULE_ATOMICSLOTMIGRATIONINFO_INITIALIZER_V1 {.version = 1}

typedef enum {
    NEXCACHEMODULE_ACL_LOG_AUTH = 0, /* Authentication failure */
    NEXCACHEMODULE_ACL_LOG_CMD,      /* Command authorization failure */
    NEXCACHEMODULE_ACL_LOG_KEY,      /* Key authorization failure */
    NEXCACHEMODULE_ACL_LOG_CHANNEL,  /* Channel authorization failure */
    NEXCACHEMODULE_ACL_LOG_DB        /* Database authorization failure */
} NexCacheModuleACLLogEntryReason;

/* Incomplete structures needed by both the core and modules. */
typedef struct NexCacheModuleCtx NexCacheModuleCtx;
typedef struct NexCacheModuleIO NexCacheModuleIO;
typedef struct NexCacheModuleDigest NexCacheModuleDigest;
typedef struct NexCacheModuleInfoCtx NexCacheModuleInfoCtx;
typedef struct NexCacheModuleDefragCtx NexCacheModuleDefragCtx;

/* Function pointers needed by both the core and modules, these needs to be
 * exposed since you can't cast a function pointer to (void *). */
typedef void (*NexCacheModuleInfoFunc)(NexCacheModuleInfoCtx *ctx, int for_crash_report);
typedef void (*NexCacheModuleDefragFunc)(NexCacheModuleDefragCtx *ctx);
typedef void (*NexCacheModuleUserChangedFunc)(uint64_t client_id, void *privdata);

/* Type definitions for implementing scripting engines modules. */
typedef void NexCacheModuleScriptingEngineCtx;
typedef void NexCacheModuleScriptingEngineServerRuntimeCtx;

/* Current ABI version for scripting engine compiled functions structure. */
#define NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_COMPILED_FUNCTION_VERSION 1UL

/* This struct represents a scripting engine function that results from the
 * compilation of a script by the engine implementation.
 */
typedef struct NexCacheModuleScriptingEngineCompiledFunction {
    uint64_t version;         /* Version of this structure for ABI compat. */
    NexCacheModuleString *name; /* Function name */
    void *function;           /* Opaque object representing a function, usually it's
                                 the function compiled code. */
    NexCacheModuleString *desc; /* Function description */
    uint64_t f_flags;         /* Function flags */
} NexCacheModuleScriptingEngineCompiledFunctionV1;

#define NexCacheModuleScriptingEngineCompiledFunction NexCacheModuleScriptingEngineCompiledFunctionV1

/* Current ABI version for scripting engine memory info structure. */
#define NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_MEMORY_INFO_VERSION 1UL

/* This struct is used to return the memory information of the scripting
 * engine.
 */
typedef struct NexCacheModuleScriptingEngineMemoryInfo {
    uint64_t version;              /* Version of this structure for ABI compat. */
    size_t used_memory;            /* The memory used by the scripting engine runtime. */
    size_t engine_memory_overhead; /* The memory used by the scripting engine data structures. */
} NexCacheModuleScriptingEngineMemoryInfoV1;

#define NexCacheModuleScriptingEngineMemoryInfo NexCacheModuleScriptingEngineMemoryInfoV1

typedef enum NexCacheModuleScriptingEngineSubsystemType {
    VMSE_EVAL,
    VMSE_FUNCTION,
    VMSE_ALL
} NexCacheModuleScriptingEngineSubsystemType;

typedef enum NexCacheModuleScriptingEngineExecutionState {
    VMSE_STATE_EXECUTING,
    VMSE_STATE_KILLED,
} NexCacheModuleScriptingEngineExecutionState;

typedef enum NexCacheModuleScriptingEngineScriptFlag {
    VMSE_SCRIPT_FLAG_NO_WRITES = (1ULL << 0),
    VMSE_SCRIPT_FLAG_ALLOW_OOM = (1ULL << 1),
    VMSE_SCRIPT_FLAG_ALLOW_STALE = (1ULL << 2),
    VMSE_SCRIPT_FLAG_NO_CLUSTER = (1ULL << 3),
    VMSE_SCRIPT_FLAG_EVAL_COMPAT_MODE = (1ULL << 4), /* EVAL Script backwards compatible behavior, no shebang provided */
    VMSE_SCRIPT_FLAG_ALLOW_CROSS_SLOT = (1ULL << 5),
} NexCacheModuleScriptingEngineScriptFlag;

typedef struct NexCacheModuleScriptingEngineCallableLazyEnvReset {
    void *context;

    /*
     * Callback function used for resetting the EVAL/FUNCTION context implemented by an
     * engine. This callback will be called by a background thread when it's
     * ready for resetting the context.
     *
     * - `context`: a generic pointer to a context object, stored in the
     * callableLazyEnvReset struct.
     *
     */
    void (*engineLazyEnvResetCallback)(void *context);
} NexCacheModuleScriptingEngineCallableLazyEnvReset;

/* The callback function called when either `EVAL`, `SCRIPT LOAD`, or
 * `FUNCTION LOAD` command is called to compile the code.
 * This callback function evaluates the source code passed and produces a list
 * of pointers to the compiled functions structure.
 * In the `EVAL` and `SCRIPT LOAD` case, the list only contains a single
 * function.
 * In the `FUNCTION LOAD` case, there are as many functions as there are calls
 * to the `server.register_function` function in the source code.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 *
 * - `code`: string pointer to the source code.
 *
 * - `code_len`: The length of the code string.
 *
 * - `timeout`: timeout for the library creation (0 for no timeout).
 *
 * - `out_num_compiled_functions`: out param with the number of objects
 *   returned by this function.
 *
 * - `err` - out param with the description of error (if occurred).
 *
 * Returns an array of compiled function objects, or `NULL` if some error
 * occurred.
 */
typedef NexCacheModuleScriptingEngineCompiledFunction **(*NexCacheModuleScriptingEngineCompileCodeFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    const char *code,
    size_t code_len,
    size_t timeout,
    size_t *out_num_compiled_functions,
    NexCacheModuleString **err);

/* Version one of source code compilation interface. This API does not allow the compiler to
 * safely handle binary data. You should use a newer version of the API if possible. */
typedef NexCacheModuleScriptingEngineCompiledFunctionV1 **(*NexCacheModuleScriptingEngineCompileCodeFuncV1)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    const char *code,
    size_t timeout,
    size_t *out_num_compiled_functions,
    NexCacheModuleString **err);

/* Free the given function.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem where the function is associated with, either `EVAL`
 *   or `FUNCTION`.
 *
 * - `compiled_function`: the compiled function to be freed.
 */
typedef void (*NexCacheModuleScriptingEngineFreeFunctionFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    NexCacheModuleScriptingEngineCompiledFunction *compiled_function);

/* The callback function called when either `EVAL`, or`FCALL`, command is
 * called.
 * This callback function executes the `compiled_function` code.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `server_ctx`: the context opaque structure that represents the server-side
 *   runtime context for the function.
 *
 * - `compiled_function`: pointer to the compiled function registered by the
 *   engine.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 *
 * - `keys`: the array of key strings passed in the `FCALL` command.
 *
 * - `nkeys`: the number of elements present in the `keys` array.
 *
 * - `args`: the array of string arguments passed in the `FCALL` command.
 *
 * - `nargs`: the number of elements present in the `args` array.
 */
typedef void (*NexCacheModuleScriptingEngineCallFunctionFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineServerRuntimeCtx *server_ctx,
    NexCacheModuleScriptingEngineCompiledFunction *compiled_function,
    NexCacheModuleScriptingEngineSubsystemType type,
    NexCacheModuleString **keys,
    size_t nkeys,
    NexCacheModuleString **args,
    size_t nargs);

/* Return memory overhead for a given function, such memory is not counted as
 * engine memory but as general structs memory that hold different information
 */
typedef size_t (*NexCacheModuleScriptingEngineGetFunctionMemoryOverheadFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCompiledFunction *compiled_function);

/* The callback function called when `SCRIPT FLUSH` command is called. The
 * engine should reset the runtime environment used for EVAL scripts.
 * This callback has been replaced by `NexCacheModuleScriptingEngineResetEnvFunc`
 * callback in ABI version 3.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `async`: if has value 1 then the reset is done asynchronously through
 * the callback structure returned by this function.
 */
typedef NexCacheModuleScriptingEngineCallableLazyEnvReset *(*NexCacheModuleScriptingEngineResetEvalFuncV2)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    int async);

/* The callback function called when `SCRIPT FLUSH` or `FUNCTION FLUSH` command is called.
 * The engine should reset the runtime environment used for EVAL scripts or FUNCTION scripts.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type.
 *
 * - `async`: if has value 1 then the reset is done asynchronously through
 * the callback structure returned by this function.
 */
typedef NexCacheModuleScriptingEngineCallableLazyEnvReset *(*NexCacheModuleScriptingEngineResetEnvFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    int async);

/* Return the current used memory by the engine.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type.
 */
typedef NexCacheModuleScriptingEngineMemoryInfo (*NexCacheModuleScriptingEngineGetMemoryInfoFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type);

typedef enum NexCacheModuleScriptingEngineDebuggerEnableRet {
    VMSE_DEBUG_NOT_SUPPORTED, /* The scripting engine does not support debugging. */
    VMSE_DEBUG_ENABLED,       /* The scripting engine has enabled the debugging mode. */
    VMSE_DEBUG_ENABLE_FAIL,   /* The scripting engine failed to enable the debugging mode. */
} NexCacheModuleScriptingEngineDebuggerEnableRet;

typedef int (*NexCacheModuleScriptingEngineDebuggerCommandHandlerFunc)(
    NexCacheModuleString **argv,
    size_t argc,
    void *context);

/* Current ABI version for scripting engine debugger commands. */
#define NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_DEBUGGER_COMMAND_VERSION 1UL

/* The structure that represents the parameter of a debugger command. */
typedef struct NexCacheModuleScriptingEngineDebuggerCommandParam {
    const char *name;
    int optional;
    int variadic;

} NexCacheModuleScriptingEngineDebuggerCommandParam;

/* The structure that represents a debugger command. */
typedef struct NexCacheModuleScriptingEngineDebuggerCommand {
    uint64_t version; /* Version of this structure for ABI compat. */

    const char *name;                                              /* The command name. */
    const size_t prefix_len;                                       /* The prefix of the command name that can be used as a short name. */
    const NexCacheModuleScriptingEngineDebuggerCommandParam *params; /* The array of parameters of this command. */
    size_t params_len;                                             /* The length of the array of parameters. */
    const char *desc;                                              /* The description of the command that is shown in the help message. */
    int invisible;                                                 /* Whether this command should be hidden in the help message. */
    NexCacheModuleScriptingEngineDebuggerCommandHandlerFunc handler; /* The function pointer that implements this command. */
    void *context;                                                 /* The pointer to a context structure that is passed when invoking the command handler. */
} NexCacheModuleScriptingEngineDebuggerCommandV1;

#define NexCacheModuleScriptingEngineDebuggerCommand NexCacheModuleScriptingEngineDebuggerCommandV1

#define NEXCACHEMODULE_SCRIPTING_ENGINE_DEBUGGER_COMMAND(NAME, PREFIX, PARAMS, PARAMS_LEN, DESC, INVISIBLE, HANDLER) \
    {                                                                                                              \
        .version = NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_DEBUGGER_COMMAND_VERSION,                                     \
        .name = NAME,                                                                                              \
        .prefix_len = PREFIX,                                                                                      \
        .params = PARAMS,                                                                                          \
        .params_len = PARAMS_LEN,                                                                                  \
        .desc = DESC,                                                                                              \
        .invisible = INVISIBLE,                                                                                    \
        .handler = HANDLER,                                                                                        \
        .context = NULL}

#define NEXCACHEMODULE_SCRIPTING_ENGINE_DEBUGGER_COMMAND_WITH_CTX(NAME, PREFIX, PARAMS, PARAMS_LEN, DESC, INVISIBLE, HANDLER, CTX) \
    {                                                                                                                            \
        .version = NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_DEBUGGER_COMMAND_VERSION,                                                   \
        .name = NAME,                                                                                                            \
        .prefix_len = PREFIX,                                                                                                    \
        .params = PARAMS,                                                                                                        \
        .params_len = PARAMS_LEN,                                                                                                \
        .desc = DESC,                                                                                                            \
        .invisible = INVISIBLE,                                                                                                  \
        .handler = HANDLER,                                                                                                      \
        .context = CTX}

/* The callback function called when `SCRIPT DEBUG (YES|SYNC)` command is called
 * to enable the remote debugger when executing a compiled function.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 *
 * - `commands`: the array of commands exposed by the remote debugger
 *   implemented by this scripting engine.
 *
 * - `commands_len`: the length of the commands array.
 *
 * Returns an enum value of type `NexCacheModuleScriptingEngineDebuggerEnableRet`.
 * Check the enum comments for more details.
 */
typedef NexCacheModuleScriptingEngineDebuggerEnableRet (*NexCacheModuleScriptingEngineDebuggerEnableFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    const NexCacheModuleScriptingEngineDebuggerCommand **commands,
    size_t *commands_length);

/* The callback function called when `SCRIPT DEBUG NO` command is called to
 * disable the remote debugger.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 */
typedef void (*NexCacheModuleScriptingEngineDebuggerDisableFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type);

/* The callback function called just before the execution of a compiled function
 * when the debugging mode is enabled.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 *
 * - `source`: the original source code from where the code of the compiled
 *   function was compiled.
 */
typedef void (*NexCacheModuleScriptingEngineDebuggerStartFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type,
    NexCacheModuleString *source);

/* The callback function called just after the execution of a compiled function
 * when the debugging mode is enabled.
 *
 * - `module_ctx`: the module runtime context.
 *
 * - `engine_ctx`: the scripting engine runtime context.
 *
 * - `type`: the subsystem type. Either EVAL or FUNCTION.
 */
typedef void (*NexCacheModuleScriptingEngineDebuggerEndFunc)(
    NexCacheModuleCtx *module_ctx,
    NexCacheModuleScriptingEngineCtx *engine_ctx,
    NexCacheModuleScriptingEngineSubsystemType type);

/* Current ABI version for scripting engine modules. */
/* Version Changelog:
 *  1. Initial version.
 *  2. Changed the `compile_code` callback to support binary data in the source code.
 *  3. Renamed reset_eval_env callback to reset_env and added a type parameter to be
 *     able to reset both EVAL or FUNCTION scripts env.
 *  4. Added support for new debugging commands.
 */
#define NEXCACHEMODULE_SCRIPTING_ENGINE_ABI_VERSION 4UL

#define NEXCACHEMODULE_SCRIPTING_ENGINE_METHODS_STRUCT_FIELDS_V3                                 \
    struct {                                                                                   \
        /* Compile code function callback. When a new script is loaded, this                   \
         * callback will be called with the script code, compiles it, and returns a            \
         * list of `NexCacheModuleScriptingEngineCompiledFunc` objects. */                       \
        union {                                                                                \
            NexCacheModuleScriptingEngineCompileCodeFuncV1 compile_code_v1;                      \
            NexCacheModuleScriptingEngineCompileCodeFunc compile_code;                           \
        };                                                                                     \
                                                                                               \
        /* Function callback to free the memory of a registered engine function. */            \
        NexCacheModuleScriptingEngineFreeFunctionFunc free_function;                             \
                                                                                               \
                                                                                               \
        /* The callback function called when `FCALL` command is called on a function           \
         * registered in this engine. */                                                       \
        NexCacheModuleScriptingEngineCallFunctionFunc call_function;                             \
                                                                                               \
        /* Function callback to return memory overhead for a given function. */                \
        NexCacheModuleScriptingEngineGetFunctionMemoryOverheadFunc get_function_memory_overhead; \
                                                                                               \
        /* The callback function used to reset the runtime environment used                    \
         * by the scripting engine for EVAL scripts or FUNCTION scripts. */                    \
        union {                                                                                \
            NexCacheModuleScriptingEngineResetEvalFuncV2 reset_eval_env_v2;                      \
            NexCacheModuleScriptingEngineResetEnvFunc reset_env;                                 \
        };                                                                                     \
                                                                                               \
        /* Function callback to get the used memory by the engine. */                          \
        NexCacheModuleScriptingEngineGetMemoryInfoFunc get_memory_info;                          \
    }

typedef struct NexCacheModuleScriptingEngineMethods {
    uint64_t version; /* Version of this structure for ABI compat. */

    NEXCACHEMODULE_SCRIPTING_ENGINE_METHODS_STRUCT_FIELDS_V3;

} NexCacheModuleScriptingEngineMethodsV3;

typedef struct NexCacheModuleScriptingEngineMethodsV4 {
    uint64_t version; /* Version of this structure for ABI compat. */

    NEXCACHEMODULE_SCRIPTING_ENGINE_METHODS_STRUCT_FIELDS_V3;

    /* Function callback to enable the debugger for the future execution of scripts. */
    NexCacheModuleScriptingEngineDebuggerEnableFunc debugger_enable;

    /* Function callback to disable the debugger. */
    NexCacheModuleScriptingEngineDebuggerDisableFunc debugger_disable;

    /* Function callback to start the debugger on a particular script. */
    NexCacheModuleScriptingEngineDebuggerStartFunc debugger_start;

    /* Function callback to end the debugger on a particular script. */
    NexCacheModuleScriptingEngineDebuggerEndFunc debugger_end;


} NexCacheModuleScriptingEngineMethodsV4;

#define NexCacheModuleScriptingEngineMethods NexCacheModuleScriptingEngineMethodsV4

/* ------------------------- End of common defines ------------------------ */

/* ----------- The rest of the defines are only for modules ----------------- */
#if !defined NEXCACHEMODULE_CORE || defined NEXCACHEMODULE_CORE_MODULE
/* Things defined for modules and core-modules. */

/* Macro definitions specific to individual compilers */
#ifndef NEXCACHEMODULE_ATTR_UNUSED
#ifdef __GNUC__
#define NEXCACHEMODULE_ATTR_UNUSED __attribute__((unused))
#else
#define NEXCACHEMODULE_ATTR_UNUSED
#endif
#endif

#ifndef NEXCACHEMODULE_ATTR_PRINTF
#ifdef __GNUC__
#define NEXCACHEMODULE_ATTR_PRINTF(idx, cnt) __attribute__((format(printf, idx, cnt)))
#else
#define NEXCACHEMODULE_ATTR_PRINTF(idx, cnt)
#endif
#endif

#ifndef NEXCACHEMODULE_ATTR_COMMON
#if defined(__GNUC__) && !(defined(__clang__) && defined(__cplusplus))
#define NEXCACHEMODULE_ATTR_COMMON __attribute__((__common__))
#else
#define NEXCACHEMODULE_ATTR_COMMON
#endif
#endif

/* Incomplete structures for compiler checks but opaque access. */
typedef struct NexCacheModuleCommand NexCacheModuleCommand;
typedef struct NexCacheModuleCallReply NexCacheModuleCallReply;
typedef struct NexCacheModuleType NexCacheModuleType;
typedef struct NexCacheModuleBlockedClient NexCacheModuleBlockedClient;
typedef struct NexCacheModuleClusterInfo NexCacheModuleClusterInfo;
typedef struct NexCacheModuleDict NexCacheModuleDict;
typedef struct NexCacheModuleDictIter NexCacheModuleDictIter;
typedef struct NexCacheModuleCommandFilterCtx NexCacheModuleCommandFilterCtx;
typedef struct NexCacheModuleCommandFilter NexCacheModuleCommandFilter;
typedef struct NexCacheModuleServerInfoData NexCacheModuleServerInfoData;
typedef struct NexCacheModuleScanCursor NexCacheModuleScanCursor;
typedef struct NexCacheModuleUser NexCacheModuleUser;
typedef struct NexCacheModuleKeyOptCtx NexCacheModuleKeyOptCtx;
typedef struct NexCacheModuleRdbStream NexCacheModuleRdbStream;

typedef int (*NexCacheModuleCmdFunc)(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc);
typedef void (*NexCacheModuleDisconnectFunc)(NexCacheModuleCtx *ctx, NexCacheModuleBlockedClient *bc);
typedef int (*NexCacheModuleNotificationFunc)(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key);
typedef void (*NexCacheModulePostNotificationJobFunc)(NexCacheModuleCtx *ctx, void *pd);
typedef void *(*NexCacheModuleTypeLoadFunc)(NexCacheModuleIO *rdb, int encver);
typedef void (*NexCacheModuleTypeSaveFunc)(NexCacheModuleIO *rdb, void *value);
typedef int (*NexCacheModuleTypeAuxLoadFunc)(NexCacheModuleIO *rdb, int encver, int when);
typedef void (*NexCacheModuleTypeAuxSaveFunc)(NexCacheModuleIO *rdb, int when);
typedef void (*NexCacheModuleTypeRewriteFunc)(NexCacheModuleIO *aof, NexCacheModuleString *key, void *value);
typedef size_t (*NexCacheModuleTypeMemUsageFunc)(const void *value);
typedef size_t (*NexCacheModuleTypeMemUsageFunc2)(NexCacheModuleKeyOptCtx *ctx, const void *value, size_t sample_size);
typedef void (*NexCacheModuleTypeDigestFunc)(NexCacheModuleDigest *digest, void *value);
typedef void (*NexCacheModuleTypeFreeFunc)(void *value);
typedef size_t (*NexCacheModuleTypeFreeEffortFunc)(NexCacheModuleString *key, const void *value);
typedef size_t (*NexCacheModuleTypeFreeEffortFunc2)(NexCacheModuleKeyOptCtx *ctx, const void *value);
typedef void (*NexCacheModuleTypeUnlinkFunc)(NexCacheModuleString *key, const void *value);
typedef void (*NexCacheModuleTypeUnlinkFunc2)(NexCacheModuleKeyOptCtx *ctx, const void *value);
typedef void *(*NexCacheModuleTypeCopyFunc)(NexCacheModuleString *fromkey, NexCacheModuleString *tokey, const void *value);
typedef void *(*NexCacheModuleTypeCopyFunc2)(NexCacheModuleKeyOptCtx *ctx, const void *value);
typedef int (*NexCacheModuleTypeDefragFunc)(NexCacheModuleDefragCtx *ctx, NexCacheModuleString *key, void **value);
typedef void (*NexCacheModuleClusterMessageReceiver)(NexCacheModuleCtx *ctx,
                                                   const char *sender_id,
                                                   uint8_t type,
                                                   const unsigned char *payload,
                                                   uint32_t len);
typedef void (*NexCacheModuleTimerProc)(NexCacheModuleCtx *ctx, void *data);
typedef void (*NexCacheModuleCommandFilterFunc)(NexCacheModuleCommandFilterCtx *filter);
typedef void (*NexCacheModuleForkDoneHandler)(int exitcode, int bysignal, void *user_data);
typedef void (*NexCacheModuleScanCB)(NexCacheModuleCtx *ctx,
                                   NexCacheModuleString *keyname,
                                   NexCacheModuleKey *key,
                                   void *privdata);
typedef void (*NexCacheModuleScanKeyCB)(NexCacheModuleKey *key,
                                      NexCacheModuleString *field,
                                      NexCacheModuleString *value,
                                      void *privdata);
typedef NexCacheModuleString *(*NexCacheModuleConfigGetStringFunc)(const char *name, void *privdata);
typedef long long (*NexCacheModuleConfigGetNumericFunc)(const char *name, void *privdata);
typedef int (*NexCacheModuleConfigGetBoolFunc)(const char *name, void *privdata);
typedef int (*NexCacheModuleConfigGetEnumFunc)(const char *name, void *privdata);
typedef int (*NexCacheModuleConfigSetStringFunc)(const char *name,
                                               NexCacheModuleString *val,
                                               void *privdata,
                                               NexCacheModuleString **err);
typedef int (*NexCacheModuleConfigSetNumericFunc)(const char *name,
                                                long long val,
                                                void *privdata,
                                                NexCacheModuleString **err);
typedef int (*NexCacheModuleConfigSetBoolFunc)(const char *name, int val, void *privdata, NexCacheModuleString **err);
typedef int (*NexCacheModuleConfigSetEnumFunc)(const char *name, int val, void *privdata, NexCacheModuleString **err);
typedef int (*NexCacheModuleConfigApplyFunc)(NexCacheModuleCtx *ctx, void *privdata, NexCacheModuleString **err);
typedef void (*NexCacheModuleOnUnblocked)(NexCacheModuleCtx *ctx, NexCacheModuleCallReply *reply, void *private_data);
typedef int (*NexCacheModuleAuthCallback)(NexCacheModuleCtx *ctx,
                                        NexCacheModuleString *username,
                                        NexCacheModuleString *password,
                                        NexCacheModuleString **err);

typedef struct NexCacheModuleTypeMethods {
    uint64_t version;
    NexCacheModuleTypeLoadFunc rdb_load;
    NexCacheModuleTypeSaveFunc rdb_save;
    NexCacheModuleTypeRewriteFunc aof_rewrite;
    NexCacheModuleTypeMemUsageFunc mem_usage;
    NexCacheModuleTypeDigestFunc digest;
    NexCacheModuleTypeFreeFunc free;
    NexCacheModuleTypeAuxLoadFunc aux_load;
    NexCacheModuleTypeAuxSaveFunc aux_save;
    int aux_save_triggers;
    NexCacheModuleTypeFreeEffortFunc free_effort;
    NexCacheModuleTypeUnlinkFunc unlink;
    NexCacheModuleTypeCopyFunc copy;
    NexCacheModuleTypeDefragFunc defrag;
    NexCacheModuleTypeMemUsageFunc2 mem_usage2;
    NexCacheModuleTypeFreeEffortFunc2 free_effort2;
    NexCacheModuleTypeUnlinkFunc2 unlink2;
    NexCacheModuleTypeCopyFunc2 copy2;
    NexCacheModuleTypeAuxSaveFunc aux_save2;
} NexCacheModuleTypeMethods;

#define NEXCACHEMODULE_GET_API(name) NexCacheModule_GetApi("NexCacheModule_" #name, ((void **)&NexCacheModule_##name))

/* Default API declaration prefix (not 'extern' for backwards compatibility) */
#ifndef NEXCACHEMODULE_API
#define NEXCACHEMODULE_API
#endif

/* Default API declaration suffix (compiler attributes) */
#ifndef NEXCACHEMODULE_ATTR
#define NEXCACHEMODULE_ATTR NEXCACHEMODULE_ATTR_COMMON
#endif

NEXCACHEMODULE_API void *(*NexCacheModule_Alloc)(size_t bytes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_TryAlloc)(size_t bytes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_Realloc)(void *ptr, size_t bytes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_TryRealloc)(void *ptr, size_t bytes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_Free)(void *ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_Calloc)(size_t nmemb, size_t size)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_TryCalloc)(size_t nmemb, size_t size)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API char *(*NexCacheModule_Strdup)(const char *str)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetApi)(const char *, void *) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CreateCommand)(NexCacheModuleCtx *ctx,
                                                   const char *name,
                                                   NexCacheModuleCmdFunc cmdfunc,
                                                   const char *strflags,
                                                   int firstkey,
                                                   int lastkey,
                                                   int keystep) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCommand *(*NexCacheModule_GetCommand)(NexCacheModuleCtx *ctx,
                                                                 const char *name)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CreateSubcommand)(NexCacheModuleCommand *parent,
                                                      const char *name,
                                                      NexCacheModuleCmdFunc cmdfunc,
                                                      const char *strflags,
                                                      int firstkey,
                                                      int lastkey,
                                                      int keystep) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetCommandInfo)(NexCacheModuleCommand *command,
                                                    const NexCacheModuleCommandInfo *info) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetCommandACLCategories)(NexCacheModuleCommand *command,
                                                             const char *ctgrsflags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AddACLCategory)(NexCacheModuleCtx *ctx, const char *name) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SetModuleAttribs)(NexCacheModuleCtx *ctx, const char *name, int ver, int apiver)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsModuleNameBusy)(const char *name) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_WrongArity)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_UpdateRuntimeArgs)(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithLongLong)(NexCacheModuleCtx *ctx, long long ll) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetSelectedDb)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SelectDb)(NexCacheModuleCtx *ctx, int newid) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_KeyExists)(NexCacheModuleCtx *ctx, NexCacheModuleString *keyname) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleKey *(*NexCacheModule_OpenKey)(NexCacheModuleCtx *ctx,
                                                          NexCacheModuleString *keyname,
                                                          int mode)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetOpenKeyModesAll)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_CloseKey)(NexCacheModuleKey *kp) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_KeyType)(NexCacheModuleKey *kp) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_ValueLength)(NexCacheModuleKey *kp) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ListPush)(NexCacheModuleKey *kp,
                                              int where,
                                              NexCacheModuleString *ele) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_ListPop)(NexCacheModuleKey *key, int where)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_ListGet)(NexCacheModuleKey *key, long index)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ListSet)(NexCacheModuleKey *key,
                                             long index,
                                             NexCacheModuleString *value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ListInsert)(NexCacheModuleKey *key,
                                                long index,
                                                NexCacheModuleString *value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ListDelete)(NexCacheModuleKey *key, long index) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCallReply *(*NexCacheModule_Call)(NexCacheModuleCtx *ctx,
                                                             const char *cmdname,
                                                             const char *fmt,
                                                             ...)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_CallReplyProto)(NexCacheModuleCallReply *reply, size_t *len)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeCallReply)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CallReplyType)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API long long (*NexCacheModule_CallReplyInteger)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API double (*NexCacheModule_CallReplyDouble)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CallReplyBool)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_CallReplyBigNumber)(NexCacheModuleCallReply *reply,
                                                                size_t *len)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_CallReplyVerbatim)(NexCacheModuleCallReply *reply,
                                                               size_t *len,
                                                               const char **format)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCallReply *(*NexCacheModule_CallReplySetElement)(NexCacheModuleCallReply *reply,
                                                                            size_t idx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CallReplyMapElement)(NexCacheModuleCallReply *reply,
                                                         size_t idx,
                                                         NexCacheModuleCallReply **key,
                                                         NexCacheModuleCallReply **val) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CallReplyAttributeElement)(NexCacheModuleCallReply *reply,
                                                               size_t idx,
                                                               NexCacheModuleCallReply **key,
                                                               NexCacheModuleCallReply **val) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_CallReplyPromiseSetUnblockHandler)(NexCacheModuleCallReply *reply,
                                                                        NexCacheModuleOnUnblocked on_unblock,
                                                                        void *private_data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CallReplyPromiseAbort)(NexCacheModuleCallReply *reply,
                                                           void **private_data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCallReply *(*NexCacheModule_CallReplyAttribute)(NexCacheModuleCallReply *reply)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_CallReplyLength)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCallReply *(*NexCacheModule_CallReplyArrayElement)(NexCacheModuleCallReply *reply,
                                                                              size_t idx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateString)(NexCacheModuleCtx *ctx,
                                                                  const char *ptr,
                                                                  size_t len)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringFromLongLong)(NexCacheModuleCtx *ctx,
                                                                              long long ll)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringFromULongLong)(NexCacheModuleCtx *ctx,
                                                                               unsigned long long ull)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringFromDouble)(NexCacheModuleCtx *ctx,
                                                                            double d)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringFromLongDouble)(NexCacheModuleCtx *ctx,
                                                                                long double ld,
                                                                                int humanfriendly)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(
    *NexCacheModule_CreateStringFromString)(NexCacheModuleCtx *ctx, const NexCacheModuleString *str)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(
    *NexCacheModule_CreateStringFromStreamID)(NexCacheModuleCtx *ctx, const NexCacheModuleStreamID *id)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringPrintf)(NexCacheModuleCtx *ctx, const char *fmt, ...)
    NEXCACHEMODULE_ATTR_PRINTF(2, 3) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeString)(NexCacheModuleCtx *ctx, NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_StringPtrLen)(const NexCacheModuleString *str, size_t *len)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithError)(NexCacheModuleCtx *ctx, const char *err) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithErrorFormat)(NexCacheModuleCtx *ctx, const char *fmt, ...) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithCustomErrorFormat)(NexCacheModuleCtx *ctx, int update_error_stats, const char *fmt, ...) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithSimpleString)(NexCacheModuleCtx *ctx, const char *msg) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithArray)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithMap)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithSet)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithAttribute)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithNullArray)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithEmptyArray)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ReplySetArrayLength)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ReplySetMapLength)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ReplySetSetLength)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ReplySetAttributeLength)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ReplySetPushLength)(NexCacheModuleCtx *ctx, long len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithStringBuffer)(NexCacheModuleCtx *ctx,
                                                           const char *buf,
                                                           size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithCString)(NexCacheModuleCtx *ctx, const char *buf) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithString)(NexCacheModuleCtx *ctx, NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithEmptyString)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithVerbatimString)(NexCacheModuleCtx *ctx,
                                                             const char *buf,
                                                             size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithVerbatimStringType)(NexCacheModuleCtx *ctx,
                                                                 const char *buf,
                                                                 size_t len,
                                                                 const char *ext) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithNull)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithBool)(NexCacheModuleCtx *ctx, int b) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithLongDouble)(NexCacheModuleCtx *ctx, long double d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithDouble)(NexCacheModuleCtx *ctx, double d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithBigNumber)(NexCacheModuleCtx *ctx,
                                                        const char *bignum,
                                                        size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplyWithCallReply)(NexCacheModuleCtx *ctx,
                                                        NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringToLongLong)(const NexCacheModuleString *str, long long *ll) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringToULongLong)(const NexCacheModuleString *str,
                                                       unsigned long long *ull) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringToDouble)(const NexCacheModuleString *str, double *d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringToLongDouble)(const NexCacheModuleString *str,
                                                        long double *d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringToStreamID)(const NexCacheModuleString *str,
                                                      NexCacheModuleStreamID *id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_AutoMemory)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_Replicate)(NexCacheModuleCtx *ctx, const char *cmdname, const char *fmt, ...)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ReplicateVerbatim)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_CallReplyStringPtr)(NexCacheModuleCallReply *reply,
                                                                size_t *len)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CreateStringFromCallReply)(NexCacheModuleCallReply *reply)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DeleteKey)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_UnlinkKey)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringSet)(NexCacheModuleKey *key, NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API char *(*NexCacheModule_StringDMA)(NexCacheModuleKey *key, size_t *len, int mode)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringTruncate)(NexCacheModuleKey *key, size_t newlen) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API mstime_t (*NexCacheModule_GetExpire)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetExpire)(NexCacheModuleKey *key, mstime_t expire) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API mstime_t (*NexCacheModule_GetAbsExpire)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetAbsExpire)(NexCacheModuleKey *key, mstime_t expire) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ResetDataset)(int restart_aof, int async) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned long long (*NexCacheModule_DbSize)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_RandomKey)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetAdd)(NexCacheModuleKey *key, double score, NexCacheModuleString *ele, int *flagsptr)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetIncrby)(NexCacheModuleKey *key,
                                                double score,
                                                NexCacheModuleString *ele,
                                                int *flagsptr,
                                                double *newscore) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetScore)(NexCacheModuleKey *key,
                                               NexCacheModuleString *ele,
                                               double *score) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetRem)(NexCacheModuleKey *key,
                                             NexCacheModuleString *ele,
                                             int *deleted) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ZsetRangeStop)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetFirstInScoreRange)(NexCacheModuleKey *key,
                                                           double min,
                                                           double max,
                                                           int minex,
                                                           int maxex) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetLastInScoreRange)(NexCacheModuleKey *key,
                                                          double min,
                                                          double max,
                                                          int minex,
                                                          int maxex) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetFirstInLexRange)(NexCacheModuleKey *key,
                                                         NexCacheModuleString *min,
                                                         NexCacheModuleString *max) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetLastInLexRange)(NexCacheModuleKey *key,
                                                        NexCacheModuleString *min,
                                                        NexCacheModuleString *max) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_ZsetRangeCurrentElement)(NexCacheModuleKey *key,
                                                                             double *score)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetRangeNext)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetRangePrev)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ZsetRangeEndReached)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_HashSet)(NexCacheModuleKey *key, int flags, ...) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_HashGet)(NexCacheModuleKey *key, int flags, ...) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_HashSetStringRef)(NexCacheModuleKey *key, NexCacheModuleString *field, const char *buf, size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_HashHasStringRef)(NexCacheModuleKey *key, NexCacheModuleString *field) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamAdd)(NexCacheModuleKey *key,
                                               int flags,
                                               NexCacheModuleStreamID *id,
                                               NexCacheModuleString **argv,
                                               int64_t numfields) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamDelete)(NexCacheModuleKey *key, NexCacheModuleStreamID *id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamIteratorStart)(NexCacheModuleKey *key,
                                                         int flags,
                                                         NexCacheModuleStreamID *startid,
                                                         NexCacheModuleStreamID *endid) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamIteratorStop)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamIteratorNextID)(NexCacheModuleKey *key,
                                                          NexCacheModuleStreamID *id,
                                                          long *numfields) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamIteratorNextField)(NexCacheModuleKey *key,
                                                             NexCacheModuleString **field_ptr,
                                                             NexCacheModuleString **value_ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StreamIteratorDelete)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API long long (*NexCacheModule_StreamTrimByLength)(NexCacheModuleKey *key,
                                                              int flags,
                                                              long long length) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API long long (*NexCacheModule_StreamTrimByID)(NexCacheModuleKey *key,
                                                          int flags,
                                                          NexCacheModuleStreamID *id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsKeysPositionRequest)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_KeyAtPos)(NexCacheModuleCtx *ctx, int pos) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_KeyAtPosWithFlags)(NexCacheModuleCtx *ctx, int pos, int flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsChannelsPositionRequest)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ChannelAtPosWithFlags)(NexCacheModuleCtx *ctx, int pos, int flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned long long (*NexCacheModule_GetClientId)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetClientUserNameById)(NexCacheModuleCtx *ctx,
                                                                           uint64_t id)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_MustObeyClient)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetClientInfoById)(void *ci, uint64_t id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetClientNameById)(NexCacheModuleCtx *ctx,
                                                                       uint64_t id)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetClientNameById)(uint64_t id, NexCacheModuleString *name) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_PublishMessage)(NexCacheModuleCtx *ctx,
                                                    NexCacheModuleString *channel,
                                                    NexCacheModuleString *message) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_PublishMessageShard)(NexCacheModuleCtx *ctx,
                                                         NexCacheModuleString *channel,
                                                         NexCacheModuleString *message) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetContextFlags)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AvoidReplicaTraffic)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_PoolAlloc)(NexCacheModuleCtx *ctx, size_t bytes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleType *(*NexCacheModule_CreateDataType)(NexCacheModuleCtx *ctx,
                                                                  const char *name,
                                                                  int encver,
                                                                  NexCacheModuleTypeMethods *typemethods)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ModuleTypeSetValue)(NexCacheModuleKey *key,
                                                        NexCacheModuleType *mt,
                                                        void *value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ModuleTypeReplaceValue)(NexCacheModuleKey *key,
                                                            NexCacheModuleType *mt,
                                                            void *new_value,
                                                            void **old_value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleType *(*NexCacheModule_ModuleTypeGetType)(NexCacheModuleKey *key)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_ModuleTypeGetValue)(NexCacheModuleKey *key)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsIOError)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SetModuleOptions)(NexCacheModuleCtx *ctx, int options) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SignalModifiedKey)(NexCacheModuleCtx *ctx,
                                                       NexCacheModuleString *keyname) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveUnsigned)(NexCacheModuleIO *io, uint64_t value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API uint64_t (*NexCacheModule_LoadUnsigned)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveSigned)(NexCacheModuleIO *io, int64_t value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int64_t (*NexCacheModule_LoadSigned)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_EmitAOF)(NexCacheModuleIO *io, const char *cmdname, const char *fmt, ...)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveString)(NexCacheModuleIO *io, NexCacheModuleString *s) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveStringBuffer)(NexCacheModuleIO *io,
                                                       const char *str,
                                                       size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_LoadString)(NexCacheModuleIO *io)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API char *(*NexCacheModule_LoadStringBuffer)(NexCacheModuleIO *io, size_t *lenptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveDouble)(NexCacheModuleIO *io, double value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API double (*NexCacheModule_LoadDouble)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveFloat)(NexCacheModuleIO *io, float value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API float (*NexCacheModule_LoadFloat)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SaveLongDouble)(NexCacheModuleIO *io, long double value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API long double (*NexCacheModule_LoadLongDouble)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_LoadDataTypeFromString)(const NexCacheModuleString *str,
                                                              const NexCacheModuleType *mt)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_LoadDataTypeFromStringEncver)(const NexCacheModuleString *str,
                                                                    const NexCacheModuleType *mt,
                                                                    int encver)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_SaveDataTypeToString)(NexCacheModuleCtx *ctx,
                                                                          void *data,
                                                                          const NexCacheModuleType *mt)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_Log)(NexCacheModuleCtx *ctx, const char *level, const char *fmt, ...)
    NEXCACHEMODULE_ATTR NEXCACHEMODULE_ATTR_PRINTF(3, 4);
NEXCACHEMODULE_API void (*NexCacheModule_LogIOError)(NexCacheModuleIO *io, const char *levelstr, const char *fmt, ...)
    NEXCACHEMODULE_ATTR NEXCACHEMODULE_ATTR_PRINTF(3, 4);
NEXCACHEMODULE_API void (*NexCacheModule__Assert)(const char *estr, const char *file, int line) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_LatencyAddSample)(const char *event, mstime_t latency) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringAppendBuffer)(NexCacheModuleCtx *ctx,
                                                        NexCacheModuleString *str,
                                                        const char *buf,
                                                        size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_TrimStringAllocation)(NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_RetainString)(NexCacheModuleCtx *ctx, NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_HoldString)(NexCacheModuleCtx *ctx,
                                                                NexCacheModuleString *str)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StringCompare)(const NexCacheModuleString *a,
                                                   const NexCacheModuleString *b) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCtx *(*NexCacheModule_GetContextFromIO)(NexCacheModuleIO *io)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetKeyNameFromIO)(NexCacheModuleIO *io)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetKeyNameFromModuleKey)(NexCacheModuleKey *key)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetDbIdFromModuleKey)(NexCacheModuleKey *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetDbIdFromIO)(NexCacheModuleIO *io) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetDbIdFromOptCtx)(NexCacheModuleKeyOptCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetToDbIdFromOptCtx)(NexCacheModuleKeyOptCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetKeyNameFromOptCtx)(NexCacheModuleKeyOptCtx *ctx)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetToKeyNameFromOptCtx)(NexCacheModuleKeyOptCtx *ctx)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API mstime_t (*NexCacheModule_Milliseconds)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API uint64_t (*NexCacheModule_MonotonicMicroseconds)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API ustime_t (*NexCacheModule_Microseconds)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API ustime_t (*NexCacheModule_CachedMicroseconds)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_DigestAddStringBuffer)(NexCacheModuleDigest *md,
                                                            const char *ele,
                                                            size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_DigestAddLongLong)(NexCacheModuleDigest *md, long long ele) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_DigestEndSequence)(NexCacheModuleDigest *md) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetDbIdFromDigest)(NexCacheModuleDigest *dig) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetKeyNameFromDigest)(NexCacheModuleDigest *dig)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleDict *(*NexCacheModule_CreateDict)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeDict)(NexCacheModuleCtx *ctx, NexCacheModuleDict *d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API uint64_t (*NexCacheModule_DictSize)(NexCacheModuleDict *d) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictSetC)(NexCacheModuleDict *d, void *key, size_t keylen, void *ptr)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictReplaceC)(NexCacheModuleDict *d, void *key, size_t keylen, void *ptr)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictSet)(NexCacheModuleDict *d, NexCacheModuleString *key, void *ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictReplace)(NexCacheModuleDict *d,
                                                 NexCacheModuleString *key,
                                                 void *ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_DictGetC)(NexCacheModuleDict *d, void *key, size_t keylen, int *nokey)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_DictGet)(NexCacheModuleDict *d,
                                               NexCacheModuleString *key,
                                               int *nokey)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictDelC)(NexCacheModuleDict *d, void *key, size_t keylen, void *oldval)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictDel)(NexCacheModuleDict *d,
                                             NexCacheModuleString *key,
                                             void *oldval) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleDictIter *(*NexCacheModule_DictIteratorStartC)(NexCacheModuleDict *d,
                                                                          const char *op,
                                                                          void *key,
                                                                          size_t keylen)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleDictIter *(*NexCacheModule_DictIteratorStart)(NexCacheModuleDict *d,
                                                                         const char *op,
                                                                         NexCacheModuleString *key)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_DictIteratorStop)(NexCacheModuleDictIter *di) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictIteratorReseekC)(NexCacheModuleDictIter *di,
                                                         const char *op,
                                                         void *key,
                                                         size_t keylen) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictIteratorReseek)(NexCacheModuleDictIter *di,
                                                        const char *op,
                                                        NexCacheModuleString *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_DictNextC)(NexCacheModuleDictIter *di,
                                                 size_t *keylen,
                                                 void **dataptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_DictPrevC)(NexCacheModuleDictIter *di,
                                                 size_t *keylen,
                                                 void **dataptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_DictNext)(NexCacheModuleCtx *ctx,
                                                              NexCacheModuleDictIter *di,
                                                              void **dataptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_DictPrev)(NexCacheModuleCtx *ctx,
                                                              NexCacheModuleDictIter *di,
                                                              void **dataptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictCompareC)(NexCacheModuleDictIter *di, const char *op, void *key, size_t keylen)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DictCompare)(NexCacheModuleDictIter *di,
                                                 const char *op,
                                                 NexCacheModuleString *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterInfoFunc)(NexCacheModuleCtx *ctx, NexCacheModuleInfoFunc cb) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_RegisterAuthCallback)(NexCacheModuleCtx *ctx,
                                                           NexCacheModuleAuthCallback cb) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddSection)(NexCacheModuleInfoCtx *ctx, const char *name) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoBeginDictField)(NexCacheModuleInfoCtx *ctx, const char *name) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoEndDictField)(NexCacheModuleInfoCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddFieldString)(NexCacheModuleInfoCtx *ctx,
                                                        const char *field,
                                                        NexCacheModuleString *value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddFieldCString)(NexCacheModuleInfoCtx *ctx,
                                                         const char *field,
                                                         const char *value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddFieldDouble)(NexCacheModuleInfoCtx *ctx,
                                                        const char *field,
                                                        double value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddFieldLongLong)(NexCacheModuleInfoCtx *ctx,
                                                          const char *field,
                                                          long long value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_InfoAddFieldULongLong)(NexCacheModuleInfoCtx *ctx,
                                                           const char *field,
                                                           unsigned long long value) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleServerInfoData *(*NexCacheModule_GetServerInfo)(NexCacheModuleCtx *ctx,
                                                                           const char *section)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeServerInfo)(NexCacheModuleCtx *ctx,
                                                     NexCacheModuleServerInfoData *data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_ServerInfoGetField)(NexCacheModuleCtx *ctx,
                                                                        NexCacheModuleServerInfoData *data,
                                                                        const char *field)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_ServerInfoGetFieldC)(NexCacheModuleServerInfoData *data,
                                                                 const char *field)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API long long (*NexCacheModule_ServerInfoGetFieldSigned)(NexCacheModuleServerInfoData *data,
                                                                    const char *field,
                                                                    int *out_err) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned long long (*NexCacheModule_ServerInfoGetFieldUnsigned)(NexCacheModuleServerInfoData *data,
                                                                               const char *field,
                                                                               int *out_err) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API double (*NexCacheModule_ServerInfoGetFieldDouble)(NexCacheModuleServerInfoData *data,
                                                                 const char *field,
                                                                 int *out_err) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SubscribeToServerEvent)(NexCacheModuleCtx *ctx,
                                                            NexCacheModuleEvent event,
                                                            NexCacheModuleEventCallback callback) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetLRU)(NexCacheModuleKey *key, mstime_t lru_idle) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetLRU)(NexCacheModuleKey *key, mstime_t *lru_idle) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetLFU)(NexCacheModuleKey *key, long long lfu_freq) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetLFU)(NexCacheModuleKey *key, long long *lfu_freq) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleBlockedClient *(*NexCacheModule_BlockClientOnKeys)(NexCacheModuleCtx *ctx,
                                                                              NexCacheModuleCmdFunc reply_callback,
                                                                              NexCacheModuleCmdFunc timeout_callback,
                                                                              void (*free_privdata)(NexCacheModuleCtx *,
                                                                                                    void *),
                                                                              long long timeout_ms,
                                                                              NexCacheModuleString **keys,
                                                                              int numkeys,
                                                                              void *privdata)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleBlockedClient *(*NexCacheModule_BlockClientOnKeysWithFlags)(
    NexCacheModuleCtx *ctx,
    NexCacheModuleCmdFunc reply_callback,
    NexCacheModuleCmdFunc timeout_callback,
    void (*free_privdata)(NexCacheModuleCtx *, void *),
    long long timeout_ms,
    NexCacheModuleString **keys,
    int numkeys,
    void *privdata,
    int flags)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SignalKeyAsReady)(NexCacheModuleCtx *ctx, NexCacheModuleString *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetBlockedClientReadyKey)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleScanCursor *(*NexCacheModule_ScanCursorCreate)(void)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ScanCursorRestart)(NexCacheModuleScanCursor *cursor) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ScanCursorDestroy)(NexCacheModuleScanCursor *cursor) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_Scan)(NexCacheModuleCtx *ctx,
                                          NexCacheModuleScanCursor *cursor,
                                          NexCacheModuleScanCB fn,
                                          void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ScanKey)(NexCacheModuleKey *key,
                                             NexCacheModuleScanCursor *cursor,
                                             NexCacheModuleScanKeyCB fn,
                                             void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetContextFlagsAll)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetModuleOptionsAll)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetKeyspaceNotificationFlagsAll)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsSubEventSupported)(NexCacheModuleEvent event, uint64_t subevent) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetServerVersion)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetTypeMethodVersion)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_Yield)(NexCacheModuleCtx *ctx, int flags, const char *busy_reply) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleBlockedClient *(*NexCacheModule_BlockClient)(NexCacheModuleCtx *ctx,
                                                                        NexCacheModuleCmdFunc reply_callback,
                                                                        NexCacheModuleCmdFunc timeout_callback,
                                                                        void (*free_privdata)(NexCacheModuleCtx *,
                                                                                              void *),
                                                                        long long timeout_ms)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_BlockClientGetPrivateData)(NexCacheModuleBlockedClient *blocked_client)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_BlockClientSetPrivateData)(NexCacheModuleBlockedClient *blocked_client,
                                                                void *private_data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleBlockedClient *(*NexCacheModule_BlockClientOnAuth)(
    NexCacheModuleCtx *ctx,
    NexCacheModuleAuthCallback reply_callback,
    void (*free_privdata)(NexCacheModuleCtx *, void *))NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_UnblockClient)(NexCacheModuleBlockedClient *bc, void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsBlockedReplyRequest)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_IsBlockedTimeoutRequest)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_GetBlockedClientPrivateData)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleBlockedClient *(*NexCacheModule_GetBlockedClientHandle)(NexCacheModuleCtx *ctx)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AbortBlock)(NexCacheModuleBlockedClient *bc) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_BlockedClientMeasureTimeStart)(NexCacheModuleBlockedClient *bc) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_BlockedClientMeasureTimeEnd)(NexCacheModuleBlockedClient *bc) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCtx *(*NexCacheModule_GetThreadSafeContext)(NexCacheModuleBlockedClient *bc)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCtx *(*NexCacheModule_GetDetachedThreadSafeContext)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeThreadSafeContext)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ThreadSafeContextLock)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ThreadSafeContextTryLock)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ThreadSafeContextUnlock)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SubscribeToKeyspaceEvents)(NexCacheModuleCtx *ctx,
                                                               int types,
                                                               NexCacheModuleNotificationFunc cb) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AddPostNotificationJob)(NexCacheModuleCtx *ctx,
                                                            NexCacheModulePostNotificationJobFunc callback,
                                                            void *pd,
                                                            void (*free_pd)(void *)) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_NotifyKeyspaceEvent)(NexCacheModuleCtx *ctx,
                                                         int type,
                                                         const char *event,
                                                         NexCacheModuleString *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetNotifyKeyspaceEvents)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_BlockedClientDisconnected)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_RegisterClusterMessageReceiver)(NexCacheModuleCtx *ctx,
                                                                     uint8_t type,
                                                                     NexCacheModuleClusterMessageReceiver callback)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SendClusterMessage)(NexCacheModuleCtx *ctx,
                                                        const char *target_id,
                                                        uint8_t type,
                                                        const char *msg,
                                                        uint32_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetClusterNodeInfo)(NexCacheModuleCtx *ctx,
                                                        const char *id,
                                                        char *ip,
                                                        char *primary_id,
                                                        int *port,
                                                        int *flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetClusterNodeInfoForClient)(NexCacheModuleCtx *ctx,
                                                                 uint64_t client_id,
                                                                 const char *node_id,
                                                                 char *ip,
                                                                 char *primary_id,
                                                                 int *port,
                                                                 int *flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API char **(*NexCacheModule_GetClusterNodesList)(NexCacheModuleCtx *ctx, size_t *numnodes)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeClusterNodesList)(char **ids) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleTimerID (*NexCacheModule_CreateTimer)(NexCacheModuleCtx *ctx,
                                                                 mstime_t period,
                                                                 NexCacheModuleTimerProc callback,
                                                                 void *data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_StopTimer)(NexCacheModuleCtx *ctx,
                                               NexCacheModuleTimerID id,
                                               void **data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetTimerInfo)(NexCacheModuleCtx *ctx,
                                                  NexCacheModuleTimerID id,
                                                  uint64_t *remaining,
                                                  void **data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_GetMyClusterID)(void)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_GetClusterSize)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_GetRandomBytes)(unsigned char *dst, size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_GetRandomHexChars)(char *dst, size_t len) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SetDisconnectCallback)(NexCacheModuleBlockedClient *bc,
                                                            NexCacheModuleDisconnectFunc callback) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SetClusterFlags)(NexCacheModuleCtx *ctx, uint64_t flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned int (*NexCacheModule_ClusterKeySlotC)(const char *key, size_t keylen) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned int (*NexCacheModule_ClusterKeySlot)(NexCacheModuleString *key) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_ClusterCanonicalKeyNameInSlot)(unsigned int slot)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ExportSharedAPI)(NexCacheModuleCtx *ctx,
                                                     const char *apiname,
                                                     void *func) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_GetSharedAPI)(NexCacheModuleCtx *ctx, const char *apiname)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleCommandFilter *(*NexCacheModule_RegisterCommandFilter)(NexCacheModuleCtx *ctx,
                                                                                  NexCacheModuleCommandFilterFunc cb,
                                                                                  int flags)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_UnregisterCommandFilter)(NexCacheModuleCtx *ctx,
                                                             NexCacheModuleCommandFilter *filter) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CommandFilterArgsCount)(NexCacheModuleCommandFilterCtx *fctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_CommandFilterArgGet)(NexCacheModuleCommandFilterCtx *fctx,
                                                                         int pos)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CommandFilterArgInsert)(NexCacheModuleCommandFilterCtx *fctx,
                                                            int pos,
                                                            NexCacheModuleString *arg) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CommandFilterArgReplace)(NexCacheModuleCommandFilterCtx *fctx,
                                                             int pos,
                                                             NexCacheModuleString *arg) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_CommandFilterArgDelete)(NexCacheModuleCommandFilterCtx *fctx,
                                                            int pos) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API unsigned long long (*NexCacheModule_CommandFilterGetClientId)(NexCacheModuleCommandFilterCtx *fctx)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_Fork)(NexCacheModuleForkDoneHandler cb, void *user_data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SendChildHeartbeat)(double progress) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ExitFromChild)(int retcode) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_KillForkChild)(int child_pid) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API float (*NexCacheModule_GetUsedMemoryRatio)(void) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_MallocSize)(void *ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_MallocUsableSize)(void *ptr) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_MallocSizeString)(NexCacheModuleString *str) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API size_t (*NexCacheModule_MallocSizeDict)(NexCacheModuleDict *dict) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleUser *(*NexCacheModule_CreateModuleUser)(const char *name)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_FreeModuleUser)(NexCacheModuleUser *user) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_SetContextUser)(NexCacheModuleCtx *ctx,
                                                     const NexCacheModuleUser *user) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetModuleUserACL)(NexCacheModuleUser *user, const char *acl) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_SetModuleUserACLString)(NexCacheModuleCtx *ctx,
                                                            NexCacheModuleUser *user,
                                                            const char *acl,
                                                            NexCacheModuleString **error) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetModuleUserACLString)(NexCacheModuleUser *user)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetCurrentUserName)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleUser *(*NexCacheModule_GetModuleUserFromUserName)(NexCacheModuleString *name)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ACLCheckCommandPermissions)(NexCacheModuleUser *user,
                                                                NexCacheModuleString **argv,
                                                                int argc) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ACLCheckKeyPermissions)(NexCacheModuleUser *user,
                                                            NexCacheModuleString *key,
                                                            int flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ACLCheckChannelPermissions)(NexCacheModuleUser *user,
                                                                NexCacheModuleString *ch,
                                                                int literal) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_ACLCheckPermissions)(NexCacheModuleUser *user,
                                                         NexCacheModuleString **argv,
                                                         int argc,
                                                         int dbid,
                                                         NexCacheModuleACLLogEntryReason *denial_reason) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ACLAddLogEntry)(NexCacheModuleCtx *ctx,
                                                     NexCacheModuleUser *user,
                                                     NexCacheModuleString *object,
                                                     NexCacheModuleACLLogEntryReason reason) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_ACLAddLogEntryByUserName)(NexCacheModuleCtx *ctx,
                                                               NexCacheModuleString *user,
                                                               NexCacheModuleString *object,
                                                               NexCacheModuleACLLogEntryReason reason) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AuthenticateClientWithACLUser)(NexCacheModuleCtx *ctx,
                                                                   const char *name,
                                                                   size_t len,
                                                                   NexCacheModuleUserChangedFunc callback,
                                                                   void *privdata,
                                                                   uint64_t *client_id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_AuthenticateClientWithUser)(NexCacheModuleCtx *ctx,
                                                                NexCacheModuleUser *user,
                                                                NexCacheModuleUserChangedFunc callback,
                                                                void *privdata,
                                                                uint64_t *client_id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DeauthenticateAndCloseClient)(NexCacheModuleCtx *ctx,
                                                                  uint64_t client_id) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RedactClientCommandArgument)(NexCacheModuleCtx *ctx, int pos) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_GetClientCertificate)(NexCacheModuleCtx *ctx,
                                                                          uint64_t id)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int *(*NexCacheModule_GetCommandKeys)(NexCacheModuleCtx *ctx,
                                                     NexCacheModuleString **argv,
                                                     int argc,
                                                     int *num_keys)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int *(*NexCacheModule_GetCommandKeysWithFlags)(NexCacheModuleCtx *ctx,
                                                              NexCacheModuleString **argv,
                                                              int argc,
                                                              int *num_keys,
                                                              int **out_flags)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const char *(*NexCacheModule_GetCurrentCommandName)(NexCacheModuleCtx *ctx)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterDefragFunc)(NexCacheModuleCtx *ctx,
                                                        NexCacheModuleDefragFunc func) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void *(*NexCacheModule_DefragAlloc)(NexCacheModuleDefragCtx *ctx, void *ptr)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleString *(*NexCacheModule_DefragNexCacheModuleString)(NexCacheModuleDefragCtx *ctx,
                                                                              NexCacheModuleString *str)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DefragShouldStop)(NexCacheModuleDefragCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DefragCursorSet)(NexCacheModuleDefragCtx *ctx,
                                                     unsigned long cursor) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_DefragCursorGet)(NexCacheModuleDefragCtx *ctx,
                                                     unsigned long *cursor) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_GetDbIdFromDefragCtx)(NexCacheModuleDefragCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API const NexCacheModuleString *(*NexCacheModule_GetKeyNameFromDefragCtx)(NexCacheModuleDefragCtx *ctx)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_EventLoopAdd)(int fd, int mask, NexCacheModuleEventLoopFunc func, void *user_data)
    NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_EventLoopDel)(int fd, int mask) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_EventLoopAddOneShot)(NexCacheModuleEventLoopOneShotFunc func,
                                                         void *user_data) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterBoolConfig)(NexCacheModuleCtx *ctx,
                                                        const char *name,
                                                        int default_val,
                                                        unsigned int flags,
                                                        NexCacheModuleConfigGetBoolFunc getfn,
                                                        NexCacheModuleConfigSetBoolFunc setfn,
                                                        NexCacheModuleConfigApplyFunc applyfn,
                                                        void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterNumericConfig)(NexCacheModuleCtx *ctx,
                                                           const char *name,
                                                           long long default_val,
                                                           unsigned int flags,
                                                           long long min,
                                                           long long max,
                                                           NexCacheModuleConfigGetNumericFunc getfn,
                                                           NexCacheModuleConfigSetNumericFunc setfn,
                                                           NexCacheModuleConfigApplyFunc applyfn,
                                                           void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterStringConfig)(NexCacheModuleCtx *ctx,
                                                          const char *name,
                                                          const char *default_val,
                                                          unsigned int flags,
                                                          NexCacheModuleConfigGetStringFunc getfn,
                                                          NexCacheModuleConfigSetStringFunc setfn,
                                                          NexCacheModuleConfigApplyFunc applyfn,
                                                          void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RegisterEnumConfig)(NexCacheModuleCtx *ctx,
                                                        const char *name,
                                                        int default_val,
                                                        unsigned int flags,
                                                        const char **enum_values,
                                                        const int *int_values,
                                                        int num_enum_vals,
                                                        NexCacheModuleConfigGetEnumFunc getfn,
                                                        NexCacheModuleConfigSetEnumFunc setfn,
                                                        NexCacheModuleConfigApplyFunc applyfn,
                                                        void *privdata) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_LoadConfigs)(NexCacheModuleCtx *ctx) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API NexCacheModuleRdbStream *(*NexCacheModule_RdbStreamCreateFromFile)(const char *filename)NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API void (*NexCacheModule_RdbStreamFree)(NexCacheModuleRdbStream *stream) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RdbLoad)(NexCacheModuleCtx *ctx,
                                             NexCacheModuleRdbStream *stream,
                                             int flags) NEXCACHEMODULE_ATTR;
NEXCACHEMODULE_API int (*NexCacheModule_RdbSave)(NexCacheModuleCtx *ctx,
                                             NexCacheModuleRdbStream *stream,
                                             int flags) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API int (*NexCacheModule_RegisterScriptingEngine)(NexCacheModuleCtx *module_ctx,
                                                             const char *engine_name,
                                                             NexCacheModuleScriptingEngineCtx *engine_ctx,
                                                             NexCacheModuleScriptingEngineMethods *engine_methods) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API int (*NexCacheModule_UnregisterScriptingEngine)(NexCacheModuleCtx *module_ctx,
                                                               const char *engine_name) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API NexCacheModuleScriptingEngineExecutionState (*NexCacheModule_GetFunctionExecutionState)(NexCacheModuleScriptingEngineServerRuntimeCtx *server_ctx) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API void (*NexCacheModule_ScriptingEngineDebuggerLog)(NexCacheModuleString *msg,
                                                                 int truncate) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API void (*NexCacheModule_ScriptingEngineDebuggerLogRespReplyStr)(const char *reply) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API void (*NexCacheModule_ScriptingEngineDebuggerLogRespReply)(NexCacheModuleCallReply *reply) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API void (*NexCacheModule_ScriptingEngineDebuggerFlushLogs)(void) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API void (*NexCacheModule_ScriptingEngineDebuggerProcessCommands)(int *client_disconnected,
                                                                             NexCacheModuleString **err) NEXCACHEMODULE_ATTR;

NEXCACHEMODULE_API int (*NexCacheModule_ACLCheckKeyPrefixPermissions)(NexCacheModuleUser *user,
                                                                  const char *key,
                                                                  size_t len,
                                                                  unsigned int flags) NEXCACHEMODULE_ATTR;

#define NexCacheModule_IsAOFClient(id) ((id) == UINT64_MAX)
/* This is included inline inside each NexCache module. */
static int NexCacheModule_Init(NexCacheModuleCtx *ctx, const char *name, int ver, int apiver) NEXCACHEMODULE_ATTR_UNUSED;
static int NexCacheModule_Init(NexCacheModuleCtx *ctx, const char *name, int ver, int apiver) {
    void *getapifuncptr = ((void **)ctx)[0];
    NexCacheModule_GetApi = (int (*)(const char *, void *))(unsigned long)getapifuncptr;
    NEXCACHEMODULE_GET_API(Alloc);
    NEXCACHEMODULE_GET_API(TryAlloc);
    NEXCACHEMODULE_GET_API(Calloc);
    NEXCACHEMODULE_GET_API(TryCalloc);
    NEXCACHEMODULE_GET_API(Free);
    NEXCACHEMODULE_GET_API(Realloc);
    NEXCACHEMODULE_GET_API(TryRealloc);
    NEXCACHEMODULE_GET_API(Strdup);
    NEXCACHEMODULE_GET_API(CreateCommand);
    NEXCACHEMODULE_GET_API(GetCommand);
    NEXCACHEMODULE_GET_API(CreateSubcommand);
    NEXCACHEMODULE_GET_API(SetCommandInfo);
    NEXCACHEMODULE_GET_API(SetCommandACLCategories);
    NEXCACHEMODULE_GET_API(AddACLCategory);
    NEXCACHEMODULE_GET_API(SetModuleAttribs);
    NEXCACHEMODULE_GET_API(IsModuleNameBusy);
    NEXCACHEMODULE_GET_API(WrongArity);
    NEXCACHEMODULE_GET_API(UpdateRuntimeArgs);
    NEXCACHEMODULE_GET_API(ReplyWithLongLong);
    NEXCACHEMODULE_GET_API(ReplyWithError);
    NEXCACHEMODULE_GET_API(ReplyWithErrorFormat);
    NEXCACHEMODULE_GET_API(ReplyWithCustomErrorFormat);
    NEXCACHEMODULE_GET_API(ReplyWithSimpleString);
    NEXCACHEMODULE_GET_API(ReplyWithArray);
    NEXCACHEMODULE_GET_API(ReplyWithMap);
    NEXCACHEMODULE_GET_API(ReplyWithSet);
    NEXCACHEMODULE_GET_API(ReplyWithAttribute);
    NEXCACHEMODULE_GET_API(ReplyWithNullArray);
    NEXCACHEMODULE_GET_API(ReplyWithEmptyArray);
    NEXCACHEMODULE_GET_API(ReplySetArrayLength);
    NEXCACHEMODULE_GET_API(ReplySetMapLength);
    NEXCACHEMODULE_GET_API(ReplySetSetLength);
    NEXCACHEMODULE_GET_API(ReplySetAttributeLength);
    NEXCACHEMODULE_GET_API(ReplySetPushLength);
    NEXCACHEMODULE_GET_API(ReplyWithStringBuffer);
    NEXCACHEMODULE_GET_API(ReplyWithCString);
    NEXCACHEMODULE_GET_API(ReplyWithString);
    NEXCACHEMODULE_GET_API(ReplyWithEmptyString);
    NEXCACHEMODULE_GET_API(ReplyWithVerbatimString);
    NEXCACHEMODULE_GET_API(ReplyWithVerbatimStringType);
    NEXCACHEMODULE_GET_API(ReplyWithNull);
    NEXCACHEMODULE_GET_API(ReplyWithBool);
    NEXCACHEMODULE_GET_API(ReplyWithCallReply);
    NEXCACHEMODULE_GET_API(ReplyWithDouble);
    NEXCACHEMODULE_GET_API(ReplyWithBigNumber);
    NEXCACHEMODULE_GET_API(ReplyWithLongDouble);
    NEXCACHEMODULE_GET_API(GetSelectedDb);
    NEXCACHEMODULE_GET_API(SelectDb);
    NEXCACHEMODULE_GET_API(KeyExists);
    NEXCACHEMODULE_GET_API(OpenKey);
    NEXCACHEMODULE_GET_API(GetOpenKeyModesAll);
    NEXCACHEMODULE_GET_API(CloseKey);
    NEXCACHEMODULE_GET_API(KeyType);
    NEXCACHEMODULE_GET_API(ValueLength);
    NEXCACHEMODULE_GET_API(ListPush);
    NEXCACHEMODULE_GET_API(ListPop);
    NEXCACHEMODULE_GET_API(ListGet);
    NEXCACHEMODULE_GET_API(ListSet);
    NEXCACHEMODULE_GET_API(ListInsert);
    NEXCACHEMODULE_GET_API(ListDelete);
    NEXCACHEMODULE_GET_API(StringToLongLong);
    NEXCACHEMODULE_GET_API(StringToULongLong);
    NEXCACHEMODULE_GET_API(StringToDouble);
    NEXCACHEMODULE_GET_API(StringToLongDouble);
    NEXCACHEMODULE_GET_API(StringToStreamID);
    NEXCACHEMODULE_GET_API(Call);
    NEXCACHEMODULE_GET_API(CallReplyProto);
    NEXCACHEMODULE_GET_API(FreeCallReply);
    NEXCACHEMODULE_GET_API(CallReplyInteger);
    NEXCACHEMODULE_GET_API(CallReplyDouble);
    NEXCACHEMODULE_GET_API(CallReplyBool);
    NEXCACHEMODULE_GET_API(CallReplyBigNumber);
    NEXCACHEMODULE_GET_API(CallReplyVerbatim);
    NEXCACHEMODULE_GET_API(CallReplySetElement);
    NEXCACHEMODULE_GET_API(CallReplyMapElement);
    NEXCACHEMODULE_GET_API(CallReplyAttributeElement);
    NEXCACHEMODULE_GET_API(CallReplyPromiseSetUnblockHandler);
    NEXCACHEMODULE_GET_API(CallReplyPromiseAbort);
    NEXCACHEMODULE_GET_API(CallReplyAttribute);
    NEXCACHEMODULE_GET_API(CallReplyType);
    NEXCACHEMODULE_GET_API(CallReplyLength);
    NEXCACHEMODULE_GET_API(CallReplyArrayElement);
    NEXCACHEMODULE_GET_API(CallReplyStringPtr);
    NEXCACHEMODULE_GET_API(CreateStringFromCallReply);
    NEXCACHEMODULE_GET_API(CreateString);
    NEXCACHEMODULE_GET_API(CreateStringFromLongLong);
    NEXCACHEMODULE_GET_API(CreateStringFromULongLong);
    NEXCACHEMODULE_GET_API(CreateStringFromDouble);
    NEXCACHEMODULE_GET_API(CreateStringFromLongDouble);
    NEXCACHEMODULE_GET_API(CreateStringFromString);
    NEXCACHEMODULE_GET_API(CreateStringFromStreamID);
    NEXCACHEMODULE_GET_API(CreateStringPrintf);
    NEXCACHEMODULE_GET_API(FreeString);
    NEXCACHEMODULE_GET_API(StringPtrLen);
    NEXCACHEMODULE_GET_API(AutoMemory);
    NEXCACHEMODULE_GET_API(Replicate);
    NEXCACHEMODULE_GET_API(ReplicateVerbatim);
    NEXCACHEMODULE_GET_API(DeleteKey);
    NEXCACHEMODULE_GET_API(UnlinkKey);
    NEXCACHEMODULE_GET_API(StringSet);
    NEXCACHEMODULE_GET_API(StringDMA);
    NEXCACHEMODULE_GET_API(StringTruncate);
    NEXCACHEMODULE_GET_API(GetExpire);
    NEXCACHEMODULE_GET_API(SetExpire);
    NEXCACHEMODULE_GET_API(GetAbsExpire);
    NEXCACHEMODULE_GET_API(SetAbsExpire);
    NEXCACHEMODULE_GET_API(ResetDataset);
    NEXCACHEMODULE_GET_API(DbSize);
    NEXCACHEMODULE_GET_API(RandomKey);
    NEXCACHEMODULE_GET_API(ZsetAdd);
    NEXCACHEMODULE_GET_API(ZsetIncrby);
    NEXCACHEMODULE_GET_API(ZsetScore);
    NEXCACHEMODULE_GET_API(ZsetRem);
    NEXCACHEMODULE_GET_API(ZsetRangeStop);
    NEXCACHEMODULE_GET_API(ZsetFirstInScoreRange);
    NEXCACHEMODULE_GET_API(ZsetLastInScoreRange);
    NEXCACHEMODULE_GET_API(ZsetFirstInLexRange);
    NEXCACHEMODULE_GET_API(ZsetLastInLexRange);
    NEXCACHEMODULE_GET_API(ZsetRangeCurrentElement);
    NEXCACHEMODULE_GET_API(ZsetRangeNext);
    NEXCACHEMODULE_GET_API(ZsetRangePrev);
    NEXCACHEMODULE_GET_API(ZsetRangeEndReached);
    NEXCACHEMODULE_GET_API(HashSet);
    NEXCACHEMODULE_GET_API(HashGet);
    NEXCACHEMODULE_GET_API(HashSetStringRef);
    NEXCACHEMODULE_GET_API(HashHasStringRef);
    NEXCACHEMODULE_GET_API(StreamAdd);
    NEXCACHEMODULE_GET_API(StreamDelete);
    NEXCACHEMODULE_GET_API(StreamIteratorStart);
    NEXCACHEMODULE_GET_API(StreamIteratorStop);
    NEXCACHEMODULE_GET_API(StreamIteratorNextID);
    NEXCACHEMODULE_GET_API(StreamIteratorNextField);
    NEXCACHEMODULE_GET_API(StreamIteratorDelete);
    NEXCACHEMODULE_GET_API(StreamTrimByLength);
    NEXCACHEMODULE_GET_API(StreamTrimByID);
    NEXCACHEMODULE_GET_API(IsKeysPositionRequest);
    NEXCACHEMODULE_GET_API(KeyAtPos);
    NEXCACHEMODULE_GET_API(KeyAtPosWithFlags);
    NEXCACHEMODULE_GET_API(IsChannelsPositionRequest);
    NEXCACHEMODULE_GET_API(ChannelAtPosWithFlags);
    NEXCACHEMODULE_GET_API(GetClientId);
    NEXCACHEMODULE_GET_API(GetClientUserNameById);
    NEXCACHEMODULE_GET_API(MustObeyClient);
    NEXCACHEMODULE_GET_API(GetContextFlags);
    NEXCACHEMODULE_GET_API(AvoidReplicaTraffic);
    NEXCACHEMODULE_GET_API(PoolAlloc);
    NEXCACHEMODULE_GET_API(CreateDataType);
    NEXCACHEMODULE_GET_API(ModuleTypeSetValue);
    NEXCACHEMODULE_GET_API(ModuleTypeReplaceValue);
    NEXCACHEMODULE_GET_API(ModuleTypeGetType);
    NEXCACHEMODULE_GET_API(ModuleTypeGetValue);
    NEXCACHEMODULE_GET_API(IsIOError);
    NEXCACHEMODULE_GET_API(SetModuleOptions);
    NEXCACHEMODULE_GET_API(SignalModifiedKey);
    NEXCACHEMODULE_GET_API(SaveUnsigned);
    NEXCACHEMODULE_GET_API(LoadUnsigned);
    NEXCACHEMODULE_GET_API(SaveSigned);
    NEXCACHEMODULE_GET_API(LoadSigned);
    NEXCACHEMODULE_GET_API(SaveString);
    NEXCACHEMODULE_GET_API(SaveStringBuffer);
    NEXCACHEMODULE_GET_API(LoadString);
    NEXCACHEMODULE_GET_API(LoadStringBuffer);
    NEXCACHEMODULE_GET_API(SaveDouble);
    NEXCACHEMODULE_GET_API(LoadDouble);
    NEXCACHEMODULE_GET_API(SaveFloat);
    NEXCACHEMODULE_GET_API(LoadFloat);
    NEXCACHEMODULE_GET_API(SaveLongDouble);
    NEXCACHEMODULE_GET_API(LoadLongDouble);
    NEXCACHEMODULE_GET_API(SaveDataTypeToString);
    NEXCACHEMODULE_GET_API(LoadDataTypeFromString);
    NEXCACHEMODULE_GET_API(LoadDataTypeFromStringEncver);
    NEXCACHEMODULE_GET_API(EmitAOF);
    NEXCACHEMODULE_GET_API(Log);
    NEXCACHEMODULE_GET_API(LogIOError);
    NEXCACHEMODULE_GET_API(_Assert);
    NEXCACHEMODULE_GET_API(LatencyAddSample);
    NEXCACHEMODULE_GET_API(StringAppendBuffer);
    NEXCACHEMODULE_GET_API(TrimStringAllocation);
    NEXCACHEMODULE_GET_API(RetainString);
    NEXCACHEMODULE_GET_API(HoldString);
    NEXCACHEMODULE_GET_API(StringCompare);
    NEXCACHEMODULE_GET_API(GetContextFromIO);
    NEXCACHEMODULE_GET_API(GetKeyNameFromIO);
    NEXCACHEMODULE_GET_API(GetKeyNameFromModuleKey);
    NEXCACHEMODULE_GET_API(GetDbIdFromModuleKey);
    NEXCACHEMODULE_GET_API(GetDbIdFromIO);
    NEXCACHEMODULE_GET_API(GetKeyNameFromOptCtx);
    NEXCACHEMODULE_GET_API(GetToKeyNameFromOptCtx);
    NEXCACHEMODULE_GET_API(GetDbIdFromOptCtx);
    NEXCACHEMODULE_GET_API(GetToDbIdFromOptCtx);
    NEXCACHEMODULE_GET_API(Milliseconds);
    NEXCACHEMODULE_GET_API(MonotonicMicroseconds);
    NEXCACHEMODULE_GET_API(Microseconds);
    NEXCACHEMODULE_GET_API(CachedMicroseconds);
    NEXCACHEMODULE_GET_API(DigestAddStringBuffer);
    NEXCACHEMODULE_GET_API(DigestAddLongLong);
    NEXCACHEMODULE_GET_API(DigestEndSequence);
    NEXCACHEMODULE_GET_API(GetKeyNameFromDigest);
    NEXCACHEMODULE_GET_API(GetDbIdFromDigest);
    NEXCACHEMODULE_GET_API(CreateDict);
    NEXCACHEMODULE_GET_API(FreeDict);
    NEXCACHEMODULE_GET_API(DictSize);
    NEXCACHEMODULE_GET_API(DictSetC);
    NEXCACHEMODULE_GET_API(DictReplaceC);
    NEXCACHEMODULE_GET_API(DictSet);
    NEXCACHEMODULE_GET_API(DictReplace);
    NEXCACHEMODULE_GET_API(DictGetC);
    NEXCACHEMODULE_GET_API(DictGet);
    NEXCACHEMODULE_GET_API(DictDelC);
    NEXCACHEMODULE_GET_API(DictDel);
    NEXCACHEMODULE_GET_API(DictIteratorStartC);
    NEXCACHEMODULE_GET_API(DictIteratorStart);
    NEXCACHEMODULE_GET_API(DictIteratorStop);
    NEXCACHEMODULE_GET_API(DictIteratorReseekC);
    NEXCACHEMODULE_GET_API(DictIteratorReseek);
    NEXCACHEMODULE_GET_API(DictNextC);
    NEXCACHEMODULE_GET_API(DictPrevC);
    NEXCACHEMODULE_GET_API(DictNext);
    NEXCACHEMODULE_GET_API(DictPrev);
    NEXCACHEMODULE_GET_API(DictCompare);
    NEXCACHEMODULE_GET_API(DictCompareC);
    NEXCACHEMODULE_GET_API(RegisterInfoFunc);
    NEXCACHEMODULE_GET_API(RegisterAuthCallback);
    NEXCACHEMODULE_GET_API(InfoAddSection);
    NEXCACHEMODULE_GET_API(InfoBeginDictField);
    NEXCACHEMODULE_GET_API(InfoEndDictField);
    NEXCACHEMODULE_GET_API(InfoAddFieldString);
    NEXCACHEMODULE_GET_API(InfoAddFieldCString);
    NEXCACHEMODULE_GET_API(InfoAddFieldDouble);
    NEXCACHEMODULE_GET_API(InfoAddFieldLongLong);
    NEXCACHEMODULE_GET_API(InfoAddFieldULongLong);
    NEXCACHEMODULE_GET_API(GetServerInfo);
    NEXCACHEMODULE_GET_API(FreeServerInfo);
    NEXCACHEMODULE_GET_API(ServerInfoGetField);
    NEXCACHEMODULE_GET_API(ServerInfoGetFieldC);
    NEXCACHEMODULE_GET_API(ServerInfoGetFieldSigned);
    NEXCACHEMODULE_GET_API(ServerInfoGetFieldUnsigned);
    NEXCACHEMODULE_GET_API(ServerInfoGetFieldDouble);
    NEXCACHEMODULE_GET_API(GetClientInfoById);
    NEXCACHEMODULE_GET_API(GetClientNameById);
    NEXCACHEMODULE_GET_API(SetClientNameById);
    NEXCACHEMODULE_GET_API(PublishMessage);
    NEXCACHEMODULE_GET_API(PublishMessageShard);
    NEXCACHEMODULE_GET_API(SubscribeToServerEvent);
    NEXCACHEMODULE_GET_API(SetLRU);
    NEXCACHEMODULE_GET_API(GetLRU);
    NEXCACHEMODULE_GET_API(SetLFU);
    NEXCACHEMODULE_GET_API(GetLFU);
    NEXCACHEMODULE_GET_API(BlockClientOnKeys);
    NEXCACHEMODULE_GET_API(BlockClientOnKeysWithFlags);
    NEXCACHEMODULE_GET_API(SignalKeyAsReady);
    NEXCACHEMODULE_GET_API(GetBlockedClientReadyKey);
    NEXCACHEMODULE_GET_API(ScanCursorCreate);
    NEXCACHEMODULE_GET_API(ScanCursorRestart);
    NEXCACHEMODULE_GET_API(ScanCursorDestroy);
    NEXCACHEMODULE_GET_API(Scan);
    NEXCACHEMODULE_GET_API(ScanKey);
    NEXCACHEMODULE_GET_API(GetContextFlagsAll);
    NEXCACHEMODULE_GET_API(GetModuleOptionsAll);
    NEXCACHEMODULE_GET_API(GetKeyspaceNotificationFlagsAll);
    NEXCACHEMODULE_GET_API(IsSubEventSupported);
    NEXCACHEMODULE_GET_API(GetServerVersion);
    NEXCACHEMODULE_GET_API(GetTypeMethodVersion);
    NEXCACHEMODULE_GET_API(Yield);
    NEXCACHEMODULE_GET_API(GetThreadSafeContext);
    NEXCACHEMODULE_GET_API(GetDetachedThreadSafeContext);
    NEXCACHEMODULE_GET_API(FreeThreadSafeContext);
    NEXCACHEMODULE_GET_API(ThreadSafeContextLock);
    NEXCACHEMODULE_GET_API(ThreadSafeContextTryLock);
    NEXCACHEMODULE_GET_API(ThreadSafeContextUnlock);
    NEXCACHEMODULE_GET_API(BlockClient);
    NEXCACHEMODULE_GET_API(BlockClientGetPrivateData);
    NEXCACHEMODULE_GET_API(BlockClientSetPrivateData);
    NEXCACHEMODULE_GET_API(BlockClientOnAuth);
    NEXCACHEMODULE_GET_API(UnblockClient);
    NEXCACHEMODULE_GET_API(IsBlockedReplyRequest);
    NEXCACHEMODULE_GET_API(IsBlockedTimeoutRequest);
    NEXCACHEMODULE_GET_API(GetBlockedClientPrivateData);
    NEXCACHEMODULE_GET_API(GetBlockedClientHandle);
    NEXCACHEMODULE_GET_API(AbortBlock);
    NEXCACHEMODULE_GET_API(BlockedClientMeasureTimeStart);
    NEXCACHEMODULE_GET_API(BlockedClientMeasureTimeEnd);
    NEXCACHEMODULE_GET_API(SetDisconnectCallback);
    NEXCACHEMODULE_GET_API(SubscribeToKeyspaceEvents);
    NEXCACHEMODULE_GET_API(AddPostNotificationJob);
    NEXCACHEMODULE_GET_API(NotifyKeyspaceEvent);
    NEXCACHEMODULE_GET_API(GetNotifyKeyspaceEvents);
    NEXCACHEMODULE_GET_API(BlockedClientDisconnected);
    NEXCACHEMODULE_GET_API(RegisterClusterMessageReceiver);
    NEXCACHEMODULE_GET_API(SendClusterMessage);
    NEXCACHEMODULE_GET_API(GetClusterNodeInfo);
    NEXCACHEMODULE_GET_API(GetClusterNodeInfoForClient);
    NEXCACHEMODULE_GET_API(GetClusterNodesList);
    NEXCACHEMODULE_GET_API(FreeClusterNodesList);
    NEXCACHEMODULE_GET_API(CreateTimer);
    NEXCACHEMODULE_GET_API(StopTimer);
    NEXCACHEMODULE_GET_API(GetTimerInfo);
    NEXCACHEMODULE_GET_API(GetMyClusterID);
    NEXCACHEMODULE_GET_API(GetClusterSize);
    NEXCACHEMODULE_GET_API(GetRandomBytes);
    NEXCACHEMODULE_GET_API(GetRandomHexChars);
    NEXCACHEMODULE_GET_API(SetClusterFlags);
    NEXCACHEMODULE_GET_API(ClusterKeySlotC);
    NEXCACHEMODULE_GET_API(ClusterKeySlot);
    NEXCACHEMODULE_GET_API(ClusterCanonicalKeyNameInSlot);
    NEXCACHEMODULE_GET_API(ExportSharedAPI);
    NEXCACHEMODULE_GET_API(GetSharedAPI);
    NEXCACHEMODULE_GET_API(RegisterCommandFilter);
    NEXCACHEMODULE_GET_API(UnregisterCommandFilter);
    NEXCACHEMODULE_GET_API(CommandFilterArgsCount);
    NEXCACHEMODULE_GET_API(CommandFilterArgGet);
    NEXCACHEMODULE_GET_API(CommandFilterArgInsert);
    NEXCACHEMODULE_GET_API(CommandFilterArgReplace);
    NEXCACHEMODULE_GET_API(CommandFilterArgDelete);
    NEXCACHEMODULE_GET_API(CommandFilterGetClientId);
    NEXCACHEMODULE_GET_API(Fork);
    NEXCACHEMODULE_GET_API(SendChildHeartbeat);
    NEXCACHEMODULE_GET_API(ExitFromChild);
    NEXCACHEMODULE_GET_API(KillForkChild);
    NEXCACHEMODULE_GET_API(GetUsedMemoryRatio);
    NEXCACHEMODULE_GET_API(MallocSize);
    NEXCACHEMODULE_GET_API(MallocUsableSize);
    NEXCACHEMODULE_GET_API(MallocSizeString);
    NEXCACHEMODULE_GET_API(MallocSizeDict);
    NEXCACHEMODULE_GET_API(CreateModuleUser);
    NEXCACHEMODULE_GET_API(FreeModuleUser);
    NEXCACHEMODULE_GET_API(SetContextUser);
    NEXCACHEMODULE_GET_API(SetModuleUserACL);
    NEXCACHEMODULE_GET_API(SetModuleUserACLString);
    NEXCACHEMODULE_GET_API(GetModuleUserACLString);
    NEXCACHEMODULE_GET_API(GetCurrentUserName);
    NEXCACHEMODULE_GET_API(GetModuleUserFromUserName);
    NEXCACHEMODULE_GET_API(ACLCheckCommandPermissions);
    NEXCACHEMODULE_GET_API(ACLCheckKeyPermissions);
    NEXCACHEMODULE_GET_API(ACLCheckChannelPermissions);
    NEXCACHEMODULE_GET_API(ACLCheckPermissions);
    NEXCACHEMODULE_GET_API(ACLAddLogEntry);
    NEXCACHEMODULE_GET_API(ACLAddLogEntryByUserName);
    NEXCACHEMODULE_GET_API(DeauthenticateAndCloseClient);
    NEXCACHEMODULE_GET_API(AuthenticateClientWithACLUser);
    NEXCACHEMODULE_GET_API(AuthenticateClientWithUser);
    NEXCACHEMODULE_GET_API(RedactClientCommandArgument);
    NEXCACHEMODULE_GET_API(GetClientCertificate);
    NEXCACHEMODULE_GET_API(GetCommandKeys);
    NEXCACHEMODULE_GET_API(GetCommandKeysWithFlags);
    NEXCACHEMODULE_GET_API(GetCurrentCommandName);
    NEXCACHEMODULE_GET_API(RegisterDefragFunc);
    NEXCACHEMODULE_GET_API(DefragAlloc);
    NEXCACHEMODULE_GET_API(DefragNexCacheModuleString);
    NEXCACHEMODULE_GET_API(DefragShouldStop);
    NEXCACHEMODULE_GET_API(DefragCursorSet);
    NEXCACHEMODULE_GET_API(DefragCursorGet);
    NEXCACHEMODULE_GET_API(GetKeyNameFromDefragCtx);
    NEXCACHEMODULE_GET_API(GetDbIdFromDefragCtx);
    NEXCACHEMODULE_GET_API(EventLoopAdd);
    NEXCACHEMODULE_GET_API(EventLoopDel);
    NEXCACHEMODULE_GET_API(EventLoopAddOneShot);
    NEXCACHEMODULE_GET_API(RegisterBoolConfig);
    NEXCACHEMODULE_GET_API(RegisterNumericConfig);
    NEXCACHEMODULE_GET_API(RegisterStringConfig);
    NEXCACHEMODULE_GET_API(RegisterEnumConfig);
    NEXCACHEMODULE_GET_API(LoadConfigs);
    NEXCACHEMODULE_GET_API(RdbStreamCreateFromFile);
    NEXCACHEMODULE_GET_API(RdbStreamFree);
    NEXCACHEMODULE_GET_API(RdbLoad);
    NEXCACHEMODULE_GET_API(RdbSave);
    NEXCACHEMODULE_GET_API(RegisterScriptingEngine);
    NEXCACHEMODULE_GET_API(UnregisterScriptingEngine);
    NEXCACHEMODULE_GET_API(GetFunctionExecutionState);
    NEXCACHEMODULE_GET_API(ScriptingEngineDebuggerLog);
    NEXCACHEMODULE_GET_API(ScriptingEngineDebuggerLogRespReplyStr);
    NEXCACHEMODULE_GET_API(ScriptingEngineDebuggerLogRespReply);
    NEXCACHEMODULE_GET_API(ScriptingEngineDebuggerFlushLogs);
    NEXCACHEMODULE_GET_API(ScriptingEngineDebuggerProcessCommands);
    NEXCACHEMODULE_GET_API(ACLCheckKeyPrefixPermissions);

    if (NexCacheModule_IsModuleNameBusy && NexCacheModule_IsModuleNameBusy(name)) return NEXCACHEMODULE_ERR;
    NexCacheModule_SetModuleAttribs(ctx, name, ver, apiver);
    return NEXCACHEMODULE_OK;
}

#define NexCacheModule_Assert(_e) ((_e) ? (void)0 : (NexCacheModule__Assert(#_e, __FILE__, __LINE__), exit(1)))

#define RMAPI_FUNC_SUPPORTED(func) (func != NULL)

#endif /* NEXCACHEMODULE_CORE */
#endif /* NEXCACHEMODULE_H */
