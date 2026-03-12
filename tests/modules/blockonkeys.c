#include "nexcachemodule.h"

#include <string.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>

#define UNUSED(V) ((void) V)

#define LIST_SIZE 1024

/* The FSL (Fixed-Size List) data type is a low-budget imitation of the
 * list data type, in order to test list-like commands implemented
 * by a module.
 * Examples: FSL.PUSH, FSL.BPOP, etc. */

typedef struct {
    long long list[LIST_SIZE];
    long long length;
} fsl_t; /* Fixed-size list */

static NexCacheModuleType *fsltype = NULL;

fsl_t *fsl_type_create(void) {
    fsl_t *o;
    o = NexCacheModule_Alloc(sizeof(*o));
    o->length = 0;
    return o;
}

void fsl_type_free(fsl_t *o) {
    NexCacheModule_Free(o);
}

/* ========================== "fsltype" type methods ======================= */

void *fsl_rdb_load(NexCacheModuleIO *rdb, int encver) {
    if (encver != 0) {
        return NULL;
    }
    fsl_t *fsl = fsl_type_create();
    fsl->length = NexCacheModule_LoadUnsigned(rdb);
    for (long long i = 0; i < fsl->length; i++)
        fsl->list[i] = NexCacheModule_LoadSigned(rdb);
    return fsl;
}

void fsl_rdb_save(NexCacheModuleIO *rdb, void *value) {
    fsl_t *fsl = value;
    NexCacheModule_SaveUnsigned(rdb,fsl->length);
    for (long long i = 0; i < fsl->length; i++)
        NexCacheModule_SaveSigned(rdb, fsl->list[i]);
}

void fsl_aofrw(NexCacheModuleIO *aof, NexCacheModuleString *key, void *value) {
    fsl_t *fsl = value;
    for (long long i = 0; i < fsl->length; i++)
        NexCacheModule_EmitAOF(aof, "FSL.PUSH","sl", key, fsl->list[i]);
}

void fsl_free(void *value) {
    fsl_type_free(value);
}

/* ========================== helper methods ======================= */

/* Wrapper to the boilerplate code of opening a key, checking its type, etc.
 * Returns 0 if `keyname` exists in the dataset, but it's of the wrong type (i.e. not FSL) */
int get_fsl(NexCacheModuleCtx *ctx, NexCacheModuleString *keyname, int mode, int create, fsl_t **fsl, int reply_on_failure) {
    *fsl = NULL;
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, keyname, mode);

    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_EMPTY) {
        /* Key exists */
        if (NexCacheModule_ModuleTypeGetType(key) != fsltype) {
            /* Key is not FSL */
            NexCacheModule_CloseKey(key);
            if (reply_on_failure)
                NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
            NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, "INCR", "c", "fsl_wrong_type");
            NexCacheModule_FreeCallReply(reply);
            return 0;
        }

        *fsl = NexCacheModule_ModuleTypeGetValue(key);
        if (*fsl && !(*fsl)->length && mode & NEXCACHEMODULE_WRITE) {
            /* Key exists, but it's logically empty */
            if (create) {
                create = 0; /* No need to create, key exists in its basic state */
            } else {
                NexCacheModule_DeleteKey(key);
                *fsl = NULL;
            }
        } else {
            /* Key exists, and has elements in it - no need to create anything */
            create = 0;
        }
    }

    if (create) {
        *fsl = fsl_type_create();
        NexCacheModule_ModuleTypeSetValue(key, fsltype, *fsl);
    }

    NexCacheModule_CloseKey(key);
    return 1;
}

/* ========================== commands ======================= */

/* FSL.PUSH <key> <int> - Push an integer to the fixed-size list (to the right).
 * It must be greater than the element in the head of the list. */
int fsl_push(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);

    long long ele;
    if (NexCacheModule_StringToLongLong(argv[2],&ele) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid integer");

    fsl_t *fsl;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_WRITE, 1, &fsl, 1))
        return NEXCACHEMODULE_OK;

    if (fsl->length == LIST_SIZE)
        return NexCacheModule_ReplyWithError(ctx,"ERR list is full");

    if (fsl->length != 0 && fsl->list[fsl->length-1] >= ele)
        return NexCacheModule_ReplyWithError(ctx,"ERR new element has to be greater than the head element");

    fsl->list[fsl->length++] = ele;
    NexCacheModule_SignalKeyAsReady(ctx, argv[1]);

    NexCacheModule_ReplicateVerbatim(ctx);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

typedef struct {
    NexCacheModuleString *keyname;
    long long ele;
} timer_data_t;

static void timer_callback(NexCacheModuleCtx *ctx, void *data)
{
    timer_data_t *td = data;

    fsl_t *fsl;
    if (!get_fsl(ctx, td->keyname, NEXCACHEMODULE_WRITE, 1, &fsl, 1))
        return;

    if (fsl->length == LIST_SIZE)
        return; /* list is full */

    if (fsl->length != 0 && fsl->list[fsl->length-1] >= td->ele)
        return; /* new element has to be greater than the head element */

    fsl->list[fsl->length++] = td->ele;
    NexCacheModule_SignalKeyAsReady(ctx, td->keyname);

    NexCacheModule_Replicate(ctx, "FSL.PUSH", "sl", td->keyname, td->ele);

    NexCacheModule_FreeString(ctx, td->keyname);
    NexCacheModule_Free(td);
}

/* FSL.PUSHTIMER <key> <int> <period-in-ms> - Push the number 9000 to the fixed-size list (to the right).
 * It must be greater than the element in the head of the list. */
int fsl_pushtimer(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    if (argc != 4)
        return NexCacheModule_WrongArity(ctx);

    long long ele;
    if (NexCacheModule_StringToLongLong(argv[2],&ele) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid integer");

    long long period;
    if (NexCacheModule_StringToLongLong(argv[3],&period) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid period");

    fsl_t *fsl;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_WRITE, 1, &fsl, 1))
        return NEXCACHEMODULE_OK;

    if (fsl->length == LIST_SIZE)
        return NexCacheModule_ReplyWithError(ctx,"ERR list is full");

    timer_data_t *td = NexCacheModule_Alloc(sizeof(*td));
    td->keyname = argv[1];
    NexCacheModule_RetainString(ctx, td->keyname);
    td->ele = ele;

    NexCacheModuleTimerID id = NexCacheModule_CreateTimer(ctx, period, timer_callback, td);
    NexCacheModule_ReplyWithLongLong(ctx, id);

    return NEXCACHEMODULE_OK;
}

int bpop_reply_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleString *keyname = NexCacheModule_GetBlockedClientReadyKey(ctx);

    fsl_t *fsl;
    if (!get_fsl(ctx, keyname, NEXCACHEMODULE_WRITE, 0, &fsl, 0) || !fsl)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_Assert(fsl->length);
    NexCacheModule_ReplyWithLongLong(ctx, fsl->list[--fsl->length]);

    /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

int bpop_timeout_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

/* FSL.BPOP <key> <timeout> [NO_TO_CB]- Block clients until list has two or more elements.
 * When that happens, unblock client and pop the last two elements (from the right). */
int fsl_bpop(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3)
        return NexCacheModule_WrongArity(ctx);

    long long timeout;
    if (NexCacheModule_StringToLongLong(argv[2],&timeout) != NEXCACHEMODULE_OK || timeout < 0)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid timeout");

    int to_cb = 1;
    if (argc == 4) {
        if (strcasecmp("NO_TO_CB", NexCacheModule_StringPtrLen(argv[3], NULL)))
            return NexCacheModule_ReplyWithError(ctx,"ERR invalid argument");
        to_cb = 0;
    }

    fsl_t *fsl;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_WRITE, 0, &fsl, 1))
        return NEXCACHEMODULE_OK;

    if (!fsl) {
        NexCacheModule_BlockClientOnKeys(ctx, bpop_reply_callback, to_cb ? bpop_timeout_callback : NULL,
                                      NULL, timeout, &argv[1], 1, NULL);
    } else {
        NexCacheModule_Assert(fsl->length);
        NexCacheModule_ReplyWithLongLong(ctx, fsl->list[--fsl->length]);
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
    }

    return NEXCACHEMODULE_OK;
}

int bpopgt_reply_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleString *keyname = NexCacheModule_GetBlockedClientReadyKey(ctx);
    long long *pgt = NexCacheModule_GetBlockedClientPrivateData(ctx);

    fsl_t *fsl;
    if (!get_fsl(ctx, keyname, NEXCACHEMODULE_WRITE, 0, &fsl, 0) || !fsl)
        return NexCacheModule_ReplyWithError(ctx,"UNBLOCKED key no longer exists");

    if (fsl->list[fsl->length-1] <= *pgt)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_Assert(fsl->length);
    NexCacheModule_ReplyWithLongLong(ctx, fsl->list[--fsl->length]);
    /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

int bpopgt_timeout_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

void bpopgt_free_privdata(NexCacheModuleCtx *ctx, void *privdata) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModule_Free(privdata);
}

/* FSL.BPOPGT <key> <gt> <timeout> - Block clients until list has an element greater than <gt>.
 * When that happens, unblock client and pop the last element (from the right). */
int fsl_bpopgt(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4)
        return NexCacheModule_WrongArity(ctx);

    long long gt;
    if (NexCacheModule_StringToLongLong(argv[2],&gt) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid integer");

    long long timeout;
    if (NexCacheModule_StringToLongLong(argv[3],&timeout) != NEXCACHEMODULE_OK || timeout < 0)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid timeout");

    fsl_t *fsl;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_WRITE, 0, &fsl, 1))
        return NEXCACHEMODULE_OK;

    if (!fsl)
        return NexCacheModule_ReplyWithError(ctx,"ERR key must exist");

    if (fsl->list[fsl->length-1] <= gt) {
        /* We use malloc so the tests in blockedonkeys.tcl can check for memory leaks */
        long long *pgt = NexCacheModule_Alloc(sizeof(long long));
        *pgt = gt;
        NexCacheModule_BlockClientOnKeysWithFlags(
            ctx, bpopgt_reply_callback, bpopgt_timeout_callback,
            bpopgt_free_privdata, timeout, &argv[1], 1, pgt,
            NEXCACHEMODULE_BLOCK_UNBLOCK_DELETED);
    } else {
        NexCacheModule_Assert(fsl->length);
        NexCacheModule_ReplyWithLongLong(ctx, fsl->list[--fsl->length]);
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
    }

    return NEXCACHEMODULE_OK;
}

int bpoppush_reply_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleString *src_keyname = NexCacheModule_GetBlockedClientReadyKey(ctx);
    NexCacheModuleString *dst_keyname = NexCacheModule_GetBlockedClientPrivateData(ctx);

    fsl_t *src;
    if (!get_fsl(ctx, src_keyname, NEXCACHEMODULE_WRITE, 0, &src, 0) || !src)
        return NEXCACHEMODULE_ERR;

    fsl_t *dst;
    if (!get_fsl(ctx, dst_keyname, NEXCACHEMODULE_WRITE, 1, &dst, 0) || !dst)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_Assert(src->length);
    long long ele = src->list[--src->length];
    dst->list[dst->length++] = ele;
    NexCacheModule_SignalKeyAsReady(ctx, dst_keyname);
    /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
    NexCacheModule_ReplicateVerbatim(ctx);
    return NexCacheModule_ReplyWithLongLong(ctx, ele);
}

int bpoppush_timeout_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

void bpoppush_free_privdata(NexCacheModuleCtx *ctx, void *privdata) {
    NexCacheModule_FreeString(ctx, privdata);
}

/* FSL.BPOPPUSH <src> <dst> <timeout> - Block clients until <src> has an element.
 * When that happens, unblock client, pop the last element from <src> and push it to <dst>
 * (from the right). */
int fsl_bpoppush(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 4)
        return NexCacheModule_WrongArity(ctx);

    long long timeout;
    if (NexCacheModule_StringToLongLong(argv[3],&timeout) != NEXCACHEMODULE_OK || timeout < 0)
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid timeout");

    fsl_t *src;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_WRITE, 0, &src, 1))
        return NEXCACHEMODULE_OK;

    if (!src) {
        /* Retain string for reply callback */
        NexCacheModule_RetainString(ctx, argv[2]);
        /* Key is empty, we must block */
        NexCacheModule_BlockClientOnKeys(ctx, bpoppush_reply_callback, bpoppush_timeout_callback,
                                      bpoppush_free_privdata, timeout, &argv[1], 1, argv[2]);
    } else {
        fsl_t *dst;
        if (!get_fsl(ctx, argv[2], NEXCACHEMODULE_WRITE, 1, &dst, 1))
            return NEXCACHEMODULE_OK;

        NexCacheModule_Assert(src->length);
        long long ele = src->list[--src->length];
        dst->list[dst->length++] = ele;
        NexCacheModule_SignalKeyAsReady(ctx, argv[2]);
        NexCacheModule_ReplyWithLongLong(ctx, ele);
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
    }

    return NEXCACHEMODULE_OK;
}

/* FSL.GETALL <key> - Reply with an array containing all elements. */
int fsl_getall(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    fsl_t *fsl;
    if (!get_fsl(ctx, argv[1], NEXCACHEMODULE_READ, 0, &fsl, 1))
        return NEXCACHEMODULE_OK;

    if (!fsl)
        return NexCacheModule_ReplyWithArray(ctx, 0);

    NexCacheModule_ReplyWithArray(ctx, fsl->length);
    for (int i = 0; i < fsl->length; i++)
        NexCacheModule_ReplyWithLongLong(ctx, fsl->list[i]);
    return NEXCACHEMODULE_OK;
}

/* Callback for blockonkeys_popall */
int blockonkeys_popall_reply_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_LIST) {
        NexCacheModuleString *elem;
        long len = 0;
        NexCacheModule_ReplyWithArray(ctx, NEXCACHEMODULE_POSTPONED_ARRAY_LEN);
        while ((elem = NexCacheModule_ListPop(key, NEXCACHEMODULE_LIST_HEAD)) != NULL) {
            len++;
            NexCacheModule_ReplyWithString(ctx, elem);
            NexCacheModule_FreeString(ctx, elem);
        }
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
        NexCacheModule_ReplySetArrayLength(ctx, len);
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR Not a list");
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int blockonkeys_popall_timeout_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithError(ctx, "ERR Timeout");
}

/* BLOCKONKEYS.POPALL key
 *
 * Blocks on an empty key for up to 3 seconds. When unblocked by a list
 * operation like LPUSH, all the elements are popped and returned. Fails with an
 * error on timeout. */
int blockonkeys_popall(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    if (NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        NexCacheModule_BlockClientOnKeys(ctx, blockonkeys_popall_reply_callback,
                                      blockonkeys_popall_timeout_callback,
                                      NULL, 3000, &argv[1], 1, NULL);
    } else {
        NexCacheModule_ReplyWithError(ctx, "ERR Key not empty");
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

/* BLOCKONKEYS.LPUSH key val [val ..]
 * BLOCKONKEYS.LPUSH_UNBLOCK key val [val ..]
 *
 * A module equivalent of LPUSH. If the name LPUSH_UNBLOCK is used,
 * RM_SignalKeyAsReady() is also called. */
int blockonkeys_lpush(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    if (NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_EMPTY &&
        NexCacheModule_KeyType(key) != NEXCACHEMODULE_KEYTYPE_LIST) {
        NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    } else {
        for (int i = 2; i < argc; i++) {
            if (NexCacheModule_ListPush(key, NEXCACHEMODULE_LIST_HEAD,
                                     argv[i]) != NEXCACHEMODULE_OK) {
                NexCacheModule_CloseKey(key);
                return NexCacheModule_ReplyWithError(ctx, "ERR Push failed");
            }
        }
    }
    NexCacheModule_CloseKey(key);

    /* signal key as ready if the command is lpush_unblock */
    size_t len;
    const char *str = NexCacheModule_StringPtrLen(argv[0], &len);
    if (!strncasecmp(str, "blockonkeys.lpush_unblock", len)) {
        NexCacheModule_SignalKeyAsReady(ctx, argv[1]);
    }
    NexCacheModule_ReplicateVerbatim(ctx);
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* Callback for the BLOCKONKEYS.BLPOPN command */
int blockonkeys_blpopn_reply_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argc);
    long long n;
    NexCacheModule_StringToLongLong(argv[2], &n);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    int result;
    if (NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_LIST &&
        NexCacheModule_ValueLength(key) >= (size_t)n) {
        NexCacheModule_ReplyWithArray(ctx, n);
        for (long i = 0; i < n; i++) {
            NexCacheModuleString *elem = NexCacheModule_ListPop(key, NEXCACHEMODULE_LIST_HEAD);
            NexCacheModule_ReplyWithString(ctx, elem);
            NexCacheModule_FreeString(ctx, elem);
        }
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
        result = NEXCACHEMODULE_OK;
    } else if (NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_LIST ||
               NexCacheModule_KeyType(key) == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        const char *module_cmd = NexCacheModule_StringPtrLen(argv[0], NULL);
        if (!strcasecmp(module_cmd, "blockonkeys.blpopn_or_unblock"))
            NexCacheModule_UnblockClient(NexCacheModule_GetBlockedClientHandle(ctx), NULL);

        /* continue blocking */
        result = NEXCACHEMODULE_ERR;
    } else {
        result = NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }
    NexCacheModule_CloseKey(key);
    return result;
}

int blockonkeys_blpopn_timeout_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithError(ctx, "ERR Timeout");
}

int blockonkeys_blpopn_abort_callback(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Action aborted");
}

/* BLOCKONKEYS.BLPOPN key N
 *
 * Blocks until key has N elements and then pops them or fails after 3 seconds.
 */
int blockonkeys_blpopn(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3) return NexCacheModule_WrongArity(ctx);

    long long n, timeout = 3000LL;
    if (NexCacheModule_StringToLongLong(argv[2], &n) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "ERR Invalid N");
    }

    if (argc > 3 ) {
        if (NexCacheModule_StringToLongLong(argv[3], &timeout) != NEXCACHEMODULE_OK) {
            return NexCacheModule_ReplyWithError(ctx, "ERR Invalid timeout value");
        }
    }
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);
    int keytype = NexCacheModule_KeyType(key);
    if (keytype != NEXCACHEMODULE_KEYTYPE_EMPTY &&
        keytype != NEXCACHEMODULE_KEYTYPE_LIST) {
        NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    } else if (keytype == NEXCACHEMODULE_KEYTYPE_LIST &&
               NexCacheModule_ValueLength(key) >= (size_t)n) {
        NexCacheModule_ReplyWithArray(ctx, n);
        for (long i = 0; i < n; i++) {
            NexCacheModuleString *elem = NexCacheModule_ListPop(key, NEXCACHEMODULE_LIST_HEAD);
            NexCacheModule_ReplyWithString(ctx, elem);
            NexCacheModule_FreeString(ctx, elem);
        }
        /* I'm lazy so i'll replicate a potentially blocking command, it shouldn't block in this flow. */
        NexCacheModule_ReplicateVerbatim(ctx);
    } else {
        NexCacheModule_BlockClientOnKeys(ctx, blockonkeys_blpopn_reply_callback,
                                      timeout ? blockonkeys_blpopn_timeout_callback : blockonkeys_blpopn_abort_callback,
                                      NULL, timeout, &argv[1], 1, NULL);
    }
    NexCacheModule_CloseKey(key);
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "blockonkeys", 1, NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModuleTypeMethods tm = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = fsl_rdb_load,
        .rdb_save = fsl_rdb_save,
        .aof_rewrite = fsl_aofrw,
        .mem_usage = NULL,
        .free = fsl_free,
        .digest = NULL,
    };

    fsltype = NexCacheModule_CreateDataType(ctx, "fsltype_t", 0, &tm);
    if (fsltype == NULL)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.push",fsl_push,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.pushtimer",fsl_pushtimer,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.bpop",fsl_bpop,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.bpopgt",fsl_bpopgt,"write",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.bpoppush",fsl_bpoppush,"write",1,2,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fsl.getall",fsl_getall,"",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blockonkeys.popall", blockonkeys_popall,
                                  "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blockonkeys.lpush", blockonkeys_lpush,
                                  "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blockonkeys.lpush_unblock", blockonkeys_lpush,
                                  "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blockonkeys.blpopn", blockonkeys_blpopn,
                                  "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "blockonkeys.blpopn_or_unblock", blockonkeys_blpopn,
                                      "write", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}
