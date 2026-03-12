#include "nexcachemodule.h"

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define UNUSED(x) (void)(x)

static int n_events = 0;

static int KeySpace_NotificationModuleKeyMissExpired(NexCacheModuleCtx *ctx, int type, const char *event, NexCacheModuleString *key) {
    UNUSED(ctx);
    UNUSED(type);
    UNUSED(event);
    UNUSED(key);
    n_events++;
    return NEXCACHEMODULE_OK;
}

int test_clear_n_events(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    n_events = 0;
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int test_get_n_events(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    NexCacheModule_ReplyWithLongLong(ctx, n_events);
    return NEXCACHEMODULE_OK;
}

int test_open_key_no_effects(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc<2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    int supportedMode = NexCacheModule_GetOpenKeyModesAll();
    if (!(supportedMode & NEXCACHEMODULE_READ) || !(supportedMode & NEXCACHEMODULE_OPEN_KEY_NOEFFECTS)) {
        NexCacheModule_ReplyWithError(ctx, "OpenKey modes are not supported");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_OPEN_KEY_NOEFFECTS);
    if (!key) {
        NexCacheModule_ReplyWithError(ctx, "key not found");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_CloseKey(key);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int test_call_generic(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc<2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    const char* cmdname = NexCacheModule_StringPtrLen(argv[1], NULL);
    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, cmdname, "v", argv+2, (size_t)argc-2);
    if (reply) {
        NexCacheModule_ReplyWithCallReply(ctx, reply);
        NexCacheModule_FreeCallReply(reply);
    } else {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
    }
    return NEXCACHEMODULE_OK;
}

int test_call_info(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NexCacheModuleCallReply *reply;
    if (argc>1)
        reply = NexCacheModule_Call(ctx, "info", "s", argv[1]);
    else
        reply = NexCacheModule_Call(ctx, "info", "");
    if (reply) {
        NexCacheModule_ReplyWithCallReply(ctx, reply);
        NexCacheModule_FreeCallReply(reply);
    } else {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
    }
    return NEXCACHEMODULE_OK;
}

int test_ld_conv(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    long double ld = 0.00000000000000001L;
    const char *ldstr = "0.00000000000000001";
    NexCacheModuleString *s1 = NexCacheModule_CreateStringFromLongDouble(ctx, ld, 1);
    NexCacheModuleString *s2 =
        NexCacheModule_CreateString(ctx, ldstr, strlen(ldstr));
    if (NexCacheModule_StringCompare(s1, s2) != 0) {
        char err[4096];
        snprintf(err, 4096,
            "Failed to convert long double to string ('%s' != '%s')",
            NexCacheModule_StringPtrLen(s1, NULL),
            NexCacheModule_StringPtrLen(s2, NULL));
        NexCacheModule_ReplyWithError(ctx, err);
        goto final;
    }
    long double ld2 = 0;
    if (NexCacheModule_StringToLongDouble(s2, &ld2) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx,
            "Failed to convert string to long double");
        goto final;
    }
    if (ld2 != ld) {
        char err[4096];
        snprintf(err, 4096,
            "Failed to convert string to long double (%.40Lf != %.40Lf)",
            ld2,
            ld);
        NexCacheModule_ReplyWithError(ctx, err);
        goto final;
    }

    /* Make sure we can't convert a string that has \0 in it */
    char buf[4] = "123";
    buf[1] = '\0';
    NexCacheModuleString *s3 = NexCacheModule_CreateString(ctx, buf, 3);
    long double ld3;
    if (NexCacheModule_StringToLongDouble(s3, &ld3) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid string successfully converted to long double");
        NexCacheModule_FreeString(ctx, s3);
        goto final;
    }
    NexCacheModule_FreeString(ctx, s3);

    NexCacheModule_ReplyWithLongDouble(ctx, ld2);
final:
    NexCacheModule_FreeString(ctx, s1);
    NexCacheModule_FreeString(ctx, s2);
    return NEXCACHEMODULE_OK;
}

int test_flushall(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_ResetDataset(1, 0);
    NexCacheModule_ReplyWithCString(ctx, "Ok");
    return NEXCACHEMODULE_OK;
}

int test_dbsize(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    long long ll = NexCacheModule_DbSize(ctx);
    NexCacheModule_ReplyWithLongLong(ctx, ll);
    return NEXCACHEMODULE_OK;
}

int test_randomkey(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleString *str = NexCacheModule_RandomKey(ctx);
    NexCacheModule_ReplyWithString(ctx, str);
    NexCacheModule_FreeString(ctx, str);
    return NEXCACHEMODULE_OK;
}

int test_keyexists(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 2) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleString *key = argv[1];
    int exists = NexCacheModule_KeyExists(ctx, key);
    return NexCacheModule_ReplyWithBool(ctx, exists);
}

NexCacheModuleKey *open_key_or_reply(NexCacheModuleCtx *ctx, NexCacheModuleString *keyname, int mode) {
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, keyname, mode);
    if (!key) {
        NexCacheModule_ReplyWithError(ctx, "key not found");
        return NULL;
    }
    return key;
}

int test_getlru(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc<2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = open_key_or_reply(ctx, argv[1], NEXCACHEMODULE_READ|NEXCACHEMODULE_OPEN_KEY_NOTOUCH);
    mstime_t lru;
    NexCacheModule_GetLRU(key, &lru);
    NexCacheModule_ReplyWithLongLong(ctx, lru);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int test_setlru(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc<3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = open_key_or_reply(ctx, argv[1], NEXCACHEMODULE_READ|NEXCACHEMODULE_OPEN_KEY_NOTOUCH);
    mstime_t lru;
    if (NexCacheModule_StringToLongLong(argv[2], &lru) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "invalid idle time");
        return NEXCACHEMODULE_OK;
    }
    int was_set = NexCacheModule_SetLRU(key, lru)==NEXCACHEMODULE_OK;
    NexCacheModule_ReplyWithLongLong(ctx, was_set);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int test_getlfu(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc<2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = open_key_or_reply(ctx, argv[1], NEXCACHEMODULE_READ|NEXCACHEMODULE_OPEN_KEY_NOTOUCH);
    mstime_t lfu;
    NexCacheModule_GetLFU(key, &lfu);
    NexCacheModule_ReplyWithLongLong(ctx, lfu);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int test_setlfu(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc<3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    NexCacheModuleKey *key = open_key_or_reply(ctx, argv[1], NEXCACHEMODULE_READ|NEXCACHEMODULE_OPEN_KEY_NOTOUCH);
    mstime_t lfu;
    if (NexCacheModule_StringToLongLong(argv[2], &lfu) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "invalid freq");
        return NEXCACHEMODULE_OK;
    }
    int was_set = NexCacheModule_SetLFU(key, lfu)==NEXCACHEMODULE_OK;
    NexCacheModule_ReplyWithLongLong(ctx, was_set);
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int test_serverversion(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    (void) argv;
    (void) argc;

    int version = NexCacheModule_GetServerVersion();
    int patch = version & 0x000000ff;
    int minor = (version & 0x0000ff00) >> 8;
    int major = (version & 0x00ff0000) >> 16;

    NexCacheModuleString* vStr = NexCacheModule_CreateStringPrintf(ctx, "%d.%d.%d", major, minor, patch);
    NexCacheModule_ReplyWithString(ctx, vStr);
    NexCacheModule_FreeString(ctx, vStr);
  
    return NEXCACHEMODULE_OK;
}

int test_getclientcert(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    (void) argv;
    (void) argc;

    NexCacheModuleString *cert = NexCacheModule_GetClientCertificate(ctx,
            NexCacheModule_GetClientId(ctx));
    if (!cert) {
        NexCacheModule_ReplyWithNull(ctx);
    } else {
        NexCacheModule_ReplyWithString(ctx, cert);
        NexCacheModule_FreeString(ctx, cert);
    }

    return NEXCACHEMODULE_OK;
}

int test_clientinfo(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    (void) argv;
    (void) argc;

    NexCacheModuleClientInfoV1 ci = NEXCACHEMODULE_CLIENTINFO_INITIALIZER_V1;
    uint64_t client_id = NexCacheModule_GetClientId(ctx);

    /* Check expected result from the V1 initializer. */
    assert(ci.version == 1);
    /* Trying to populate a future version of the struct should fail. */
    ci.version = NEXCACHEMODULE_CLIENTINFO_VERSION + 1;
    assert(NexCacheModule_GetClientInfoById(&ci, client_id) == NEXCACHEMODULE_ERR);

    ci.version = 1;
    if (NexCacheModule_GetClientInfoById(&ci, client_id) == NEXCACHEMODULE_ERR) {
            NexCacheModule_ReplyWithError(ctx, "failed to get client info");
            return NEXCACHEMODULE_OK;
    }

    NexCacheModule_ReplyWithArray(ctx, 10);
    char flags[512];
    snprintf(flags, sizeof(flags) - 1, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_SSL ? "ssl" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_PUBSUB ? "pubsub" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_BLOCKED ? "blocked" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_TRACKING ? "tracking" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_UNIXSOCKET ? "unixsocket" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_MULTI ? "multi" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_READONLY ? "readonly" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_PRIMARY ? "primary" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_REPLICA ? "replica" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_MONITOR ? "monitor" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_MODULE ? "module" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_AUTHENTICATED ? "authenticated" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_EVER_AUTHENTICATED ? "ever_authenticated" : "",
        ci.flags & NEXCACHEMODULE_CLIENTINFO_FLAG_FAKE ? "fake" : "");

    NexCacheModule_ReplyWithCString(ctx, "flags");
    NexCacheModule_ReplyWithCString(ctx, flags);
    NexCacheModule_ReplyWithCString(ctx, "id");
    NexCacheModule_ReplyWithLongLong(ctx, ci.id);
    NexCacheModule_ReplyWithCString(ctx, "addr");
    NexCacheModule_ReplyWithCString(ctx, ci.addr);
    NexCacheModule_ReplyWithCString(ctx, "port");
    NexCacheModule_ReplyWithLongLong(ctx, ci.port);
    NexCacheModule_ReplyWithCString(ctx, "db");
    NexCacheModule_ReplyWithLongLong(ctx, ci.db);

    return NEXCACHEMODULE_OK;
}

int test_getname(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    (void)argv;
    if (argc != 1) return NexCacheModule_WrongArity(ctx);
    unsigned long long id = NexCacheModule_GetClientId(ctx);
    NexCacheModuleString *name = NexCacheModule_GetClientNameById(ctx, id);
    if (name == NULL)
        return NexCacheModule_ReplyWithError(ctx, "-ERR No name");
    NexCacheModule_ReplyWithString(ctx, name);
    NexCacheModule_FreeString(ctx, name);
    return NEXCACHEMODULE_OK;
}

int test_setname(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);
    unsigned long long id = NexCacheModule_GetClientId(ctx);
    if (NexCacheModule_SetClientNameById(id, argv[1]) == NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    else
        return NexCacheModule_ReplyWithError(ctx, strerror(errno));
}

int test_log_tsctx(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    NexCacheModuleCtx *tsctx = NexCacheModule_GetDetachedThreadSafeContext(ctx);

    if (argc != 3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    char level[50];
    size_t level_len;
    const char *level_str = NexCacheModule_StringPtrLen(argv[1], &level_len);
    snprintf(level, sizeof(level) - 1, "%.*s", (int) level_len, level_str);

    size_t msg_len;
    const char *msg_str = NexCacheModule_StringPtrLen(argv[2], &msg_len);

    NexCacheModule_Log(tsctx, level, "%.*s", (int) msg_len, msg_str);
    NexCacheModule_FreeThreadSafeContext(tsctx);

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int test_weird_cmd(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int test_monotonic_time(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_ReplyWithLongLong(ctx, NexCacheModule_MonotonicMicroseconds());
    return NEXCACHEMODULE_OK;
}

/* wrapper for RM_Call */
int test_rm_call(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "Ev", argv + 2, (size_t)argc - 2);
    if(!rep){
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    }else{
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

/* wrapper for RM_Call which also replicates the module command */
int test_rm_call_replicate(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    test_rm_call(ctx, argv, argc);
    NexCacheModule_ReplicateVerbatim(ctx);

    return NEXCACHEMODULE_OK;
}

/* wrapper for RM_Call with flags */
int test_rm_call_flags(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    if(argc < 3){
        return NexCacheModule_WrongArity(ctx);
    }

    /* Append Ev to the provided flags. */
    NexCacheModuleString *flags = NexCacheModule_CreateStringFromString(ctx, argv[1]);
    NexCacheModule_StringAppendBuffer(ctx, flags, "Ev", 2);

    const char* flg = NexCacheModule_StringPtrLen(flags, NULL);
    const char* cmd = NexCacheModule_StringPtrLen(argv[2], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, flg, argv + 3, (size_t)argc - 3);
    if(!rep){
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    }else{
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }
    NexCacheModule_FreeString(ctx, flags);

    return NEXCACHEMODULE_OK;
}

int test_ull_conv(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    unsigned long long ull = 18446744073709551615ULL;
    const char *ullstr = "18446744073709551615";

    NexCacheModuleString *s1 = NexCacheModule_CreateStringFromULongLong(ctx, ull);
    NexCacheModuleString *s2 =
        NexCacheModule_CreateString(ctx, ullstr, strlen(ullstr));
    if (NexCacheModule_StringCompare(s1, s2) != 0) {
        char err[4096];
        snprintf(err, 4096,
            "Failed to convert unsigned long long to string ('%s' != '%s')",
            NexCacheModule_StringPtrLen(s1, NULL),
            NexCacheModule_StringPtrLen(s2, NULL));
        NexCacheModule_ReplyWithError(ctx, err);
        goto final;
    }
    unsigned long long ull2 = 0;
    if (NexCacheModule_StringToULongLong(s2, &ull2) == NEXCACHEMODULE_ERR) {
        NexCacheModule_ReplyWithError(ctx,
            "Failed to convert string to unsigned long long");
        goto final;
    }
    if (ull2 != ull) {
        char err[4096];
        snprintf(err, 4096,
            "Failed to convert string to unsigned long long (%llu != %llu)",
            ull2,
            ull);
        NexCacheModule_ReplyWithError(ctx, err);
        goto final;
    }
    
    /* Make sure we can't convert a string more than ULLONG_MAX or less than 0 */
    ullstr = "18446744073709551616";
    NexCacheModuleString *s3 = NexCacheModule_CreateString(ctx, ullstr, strlen(ullstr));
    unsigned long long ull3;
    if (NexCacheModule_StringToULongLong(s3, &ull3) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid string successfully converted to unsigned long long");
        NexCacheModule_FreeString(ctx, s3);
        goto final;
    }
    NexCacheModule_FreeString(ctx, s3);
    ullstr = "-1";
    NexCacheModuleString *s4 = NexCacheModule_CreateString(ctx, ullstr, strlen(ullstr));
    unsigned long long ull4;
    if (NexCacheModule_StringToULongLong(s4, &ull4) == NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "Invalid string successfully converted to unsigned long long");
        NexCacheModule_FreeString(ctx, s4);
        goto final;
    }
    NexCacheModule_FreeString(ctx, s4);
   
    NexCacheModule_ReplyWithSimpleString(ctx, "ok");

final:
    NexCacheModule_FreeString(ctx, s1);
    NexCacheModule_FreeString(ctx, s2);
    return NEXCACHEMODULE_OK;
}

int test_malloc_api(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);

    void *p;

    p = NexCacheModule_TryAlloc(1024);
    memset(p, 0, 1024);
    NexCacheModule_Free(p);

    p = NexCacheModule_TryCalloc(1, 1024);
    memset(p, 1, 1024);

    p = NexCacheModule_TryRealloc(p, 5 * 1024);
    memset(p, 1, 5 * 1024);
    NexCacheModule_Free(p);

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int test_keyslot(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    /* Static check of the ClusterKeySlot + ClusterCanonicalKeyNameInSlot
     * round-trip for all slots. */
    for (unsigned int slot = 0; slot < 16384; slot++) {
        const char *tag = NexCacheModule_ClusterCanonicalKeyNameInSlot(slot);
        NexCacheModuleString *key = NexCacheModule_CreateStringPrintf(ctx, "x{%s}y", tag);
        assert(slot == NexCacheModule_ClusterKeySlot(key));

        size_t keylen;
        const char *keyptr = NexCacheModule_StringPtrLen(key, &keylen);
        assert(slot == NexCacheModule_ClusterKeySlotC(keyptr, keylen));

        NexCacheModule_FreeString(ctx, key);
    }
    if (argc != 2){
        return NexCacheModule_WrongArity(ctx);
    }
    unsigned int slot = NexCacheModule_ClusterKeySlot(argv[1]);
    return NexCacheModule_ReplyWithLongLong(ctx, slot);
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"misc",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if(NexCacheModule_SubscribeToKeyspaceEvents(ctx, NEXCACHEMODULE_NOTIFY_KEY_MISS | NEXCACHEMODULE_NOTIFY_EXPIRED, KeySpace_NotificationModuleKeyMissExpired) != NEXCACHEMODULE_OK){
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx,"test.call_generic", test_call_generic,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.call_info", test_call_info,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.ld_conversion", test_ld_conv, "",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.ull_conversion", test_ull_conv, "",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.flushall", test_flushall,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.dbsize", test_dbsize,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.randomkey", test_randomkey,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.keyexists", test_keyexists,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.setlru", test_setlru,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.getlru", test_getlru,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.setlfu", test_setlfu,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.getlfu", test_getlfu,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.clientinfo", test_clientinfo,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.getname", test_getname,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.setname", test_setname,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.serverversion", test_serverversion,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.getclientcert", test_getclientcert,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.log_tsctx", test_log_tsctx,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    /* Add a command with ':' in it's name, so that we can check commandstats sanitization. */
    if (NexCacheModule_CreateCommand(ctx,"test.weird:cmd", test_weird_cmd,"readonly",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"test.monotonic_time", test_monotonic_time,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.rm_call", test_rm_call,"allow-stale", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.rm_call_flags", test_rm_call_flags,"allow-stale", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.rm_call_replicate", test_rm_call_replicate,"allow-stale", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.silent_open_key", test_open_key_no_effects,"", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.get_n_events", test_get_n_events,"", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.clear_n_events", test_clear_n_events,"", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.malloc_api", test_malloc_api,"", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx, "test.keyslot", test_keyslot, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
