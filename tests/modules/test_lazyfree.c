/* This module emulates a linked list for lazyfree testing of modules, which
 is a simplified version of 'hellotype.c'
 */
#include "nexcachemodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

static NexCacheModuleType *LazyFreeLinkType;

struct LazyFreeLinkNode {
    int64_t value;
    struct LazyFreeLinkNode *next;
};

struct LazyFreeLinkObject {
    struct LazyFreeLinkNode *head;
    size_t len; /* Number of elements added. */
};

struct LazyFreeLinkObject *createLazyFreeLinkObject(void) {
    struct LazyFreeLinkObject *o;
    o = NexCacheModule_Alloc(sizeof(*o));
    o->head = NULL;
    o->len = 0;
    return o;
}

void LazyFreeLinkInsert(struct LazyFreeLinkObject *o, int64_t ele) {
    struct LazyFreeLinkNode *next = o->head, *newnode, *prev = NULL;

    while(next && next->value < ele) {
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

void LazyFreeLinkReleaseObject(struct LazyFreeLinkObject *o) {
    struct LazyFreeLinkNode *cur, *next;
    cur = o->head;
    while(cur) {
        next = cur->next;
        NexCacheModule_Free(cur);
        cur = next;
    }
    NexCacheModule_Free(o);
}

/* LAZYFREELINK.INSERT key value */
int LazyFreeLinkInsert_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 3) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx,argv[1],
        NEXCACHEMODULE_READ|NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY &&
        NexCacheModule_ModuleTypeGetType(key) != LazyFreeLinkType)
    {
        return NexCacheModule_ReplyWithError(ctx,NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    long long value;
    if ((NexCacheModule_StringToLongLong(argv[2],&value) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx,"ERR invalid value: must be a signed 64 bit integer");
    }

    struct LazyFreeLinkObject *hto;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        hto = createLazyFreeLinkObject();
        NexCacheModule_ModuleTypeSetValue(key,LazyFreeLinkType,hto);
    } else {
        hto = NexCacheModule_ModuleTypeGetValue(key);
    }

    LazyFreeLinkInsert(hto,value);
    NexCacheModule_SignalKeyAsReady(ctx,argv[1]);

    NexCacheModule_ReplyWithLongLong(ctx,hto->len);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* LAZYFREELINK.LEN key */
int LazyFreeLinkLen_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 2) return NexCacheModule_WrongArity(ctx);
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx,argv[1],
                                              NEXCACHEMODULE_READ);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY &&
        NexCacheModule_ModuleTypeGetType(key) != LazyFreeLinkType)
    {
        return NexCacheModule_ReplyWithError(ctx,NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    struct LazyFreeLinkObject *hto = NexCacheModule_ModuleTypeGetValue(key);
    NexCacheModule_ReplyWithLongLong(ctx,hto ? hto->len : 0);
    return NEXCACHEMODULE_OK;
}

void *LazyFreeLinkRdbLoad(NexCacheModuleIO *rdb, int encver) {
    if (encver != 0) {
        return NULL;
    }
    uint64_t elements = NexCacheModule_LoadUnsigned(rdb);
    struct LazyFreeLinkObject *hto = createLazyFreeLinkObject();
    while(elements--) {
        int64_t ele = NexCacheModule_LoadSigned(rdb);
        LazyFreeLinkInsert(hto,ele);
    }
    return hto;
}

void LazyFreeLinkRdbSave(NexCacheModuleIO *rdb, void *value) {
    struct LazyFreeLinkObject *hto = value;
    struct LazyFreeLinkNode *node = hto->head;
    NexCacheModule_SaveUnsigned(rdb,hto->len);
    while(node) {
        NexCacheModule_SaveSigned(rdb,node->value);
        node = node->next;
    }
}

void LazyFreeLinkAofRewrite(NexCacheModuleIO *aof, NexCacheModuleString *key, void *value) {
    struct LazyFreeLinkObject *hto = value;
    struct LazyFreeLinkNode *node = hto->head;
    while(node) {
        NexCacheModule_EmitAOF(aof,"LAZYFREELINK.INSERT","sl",key,node->value);
        node = node->next;
    }
}

void LazyFreeLinkFree(void *value) {
    LazyFreeLinkReleaseObject(value);
}

size_t LazyFreeLinkFreeEffort(NexCacheModuleString *key, const void *value) {
    NEXCACHEMODULE_NOT_USED(key);
    const struct LazyFreeLinkObject *hto = value;
    return hto->len;
}

void LazyFreeLinkUnlink(NexCacheModuleString *key, const void *value) {
    NEXCACHEMODULE_NOT_USED(key);
    NEXCACHEMODULE_NOT_USED(value);
    /* Here you can know which key and value is about to be freed. */
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"lazyfreetest",1,NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    /* We only allow our module to be loaded when the core version is greater than the version of my module */
    if (NexCacheModule_GetTypeMethodVersion() < NEXCACHEMODULE_TYPE_METHOD_VERSION) {
        return NEXCACHEMODULE_ERR;
    }

    NexCacheModuleTypeMethods tm = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = LazyFreeLinkRdbLoad,
        .rdb_save = LazyFreeLinkRdbSave,
        .aof_rewrite = LazyFreeLinkAofRewrite,
        .free = LazyFreeLinkFree,
        .free_effort = LazyFreeLinkFreeEffort,
        .unlink = LazyFreeLinkUnlink,
    };

    LazyFreeLinkType = NexCacheModule_CreateDataType(ctx,"test_lazy",0,&tm);
    if (LazyFreeLinkType == NULL) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"lazyfreelink.insert",
        LazyFreeLinkInsert_NexCacheCommand,"write deny-oom",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"lazyfreelink.len",
        LazyFreeLinkLen_NexCacheCommand,"readonly",1,1,1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
