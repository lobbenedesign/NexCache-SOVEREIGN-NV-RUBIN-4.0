#include "nexcachemodule.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define UNUSED(V) ((void) V)

/* Registered type */
NexCacheModuleType *mallocsize_type = NULL;

typedef enum {
    UDT_RAW,
    UDT_STRING,
    UDT_DICT
} udt_type_t;

typedef struct {
    void *ptr;
    size_t len;
} raw_t;

typedef struct {
    udt_type_t type;
    union {
        raw_t raw;
        NexCacheModuleString *str;
        NexCacheModuleDict *dict;
    } data;
} udt_t;

void udt_free(void *value) {
    udt_t *udt = value;
    switch (udt->type) {
        case (UDT_RAW): {
            NexCacheModule_Free(udt->data.raw.ptr);
            break;
        }
        case (UDT_STRING): {
            NexCacheModule_FreeString(NULL, udt->data.str);
            break;
        }
        case (UDT_DICT): {
            NexCacheModuleString *dk, *dv;
            NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(udt->data.dict, "^", NULL, 0);
            while((dk = NexCacheModule_DictNext(NULL, iter, (void **)&dv)) != NULL) {
                NexCacheModule_FreeString(NULL, dk);
                NexCacheModule_FreeString(NULL, dv);
            }
            NexCacheModule_DictIteratorStop(iter);
            NexCacheModule_FreeDict(NULL, udt->data.dict);
            break;
        }
    }
    NexCacheModule_Free(udt);
}

void udt_rdb_save(NexCacheModuleIO *rdb, void *value) {
    udt_t *udt = value;
    NexCacheModule_SaveUnsigned(rdb, udt->type);
    switch (udt->type) {
        case (UDT_RAW): {
            NexCacheModule_SaveStringBuffer(rdb, udt->data.raw.ptr, udt->data.raw.len);
            break;
        }
        case (UDT_STRING): {
            NexCacheModule_SaveString(rdb, udt->data.str);
            break;
        }
        case (UDT_DICT): {
            NexCacheModule_SaveUnsigned(rdb, NexCacheModule_DictSize(udt->data.dict));
            NexCacheModuleString *dk, *dv;
            NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(udt->data.dict, "^", NULL, 0);
            while((dk = NexCacheModule_DictNext(NULL, iter, (void **)&dv)) != NULL) {
                NexCacheModule_SaveString(rdb, dk);
                NexCacheModule_SaveString(rdb, dv);
                NexCacheModule_FreeString(NULL, dk); /* Allocated by NexCacheModule_DictNext */
            }
            NexCacheModule_DictIteratorStop(iter);
            break;
        }
    }
}

void *udt_rdb_load(NexCacheModuleIO *rdb, int encver) {
    if (encver != 0)
        return NULL;
    udt_t *udt = NexCacheModule_Alloc(sizeof(*udt));
    udt->type = NexCacheModule_LoadUnsigned(rdb);
    switch (udt->type) {
        case (UDT_RAW): {
            udt->data.raw.ptr = NexCacheModule_LoadStringBuffer(rdb, &udt->data.raw.len);
            break;
        }
        case (UDT_STRING): {
            udt->data.str = NexCacheModule_LoadString(rdb);
            break;
        }
        case (UDT_DICT): {
            long long dict_len = NexCacheModule_LoadUnsigned(rdb);
            udt->data.dict = NexCacheModule_CreateDict(NULL);
            for (int i = 0; i < dict_len; i += 2) {
                NexCacheModuleString *key = NexCacheModule_LoadString(rdb);
                NexCacheModuleString *val = NexCacheModule_LoadString(rdb);
                NexCacheModule_DictSet(udt->data.dict, key, val);
            }
            break;
        }
    }

    return udt;
}

size_t udt_mem_usage(NexCacheModuleKeyOptCtx *ctx, const void *value, size_t sample_size) {
    UNUSED(ctx);
    UNUSED(sample_size);
    
    const udt_t *udt = value;
    size_t size = sizeof(*udt);
    
    switch (udt->type) {
        case (UDT_RAW): {
            size += NexCacheModule_MallocSize(udt->data.raw.ptr);
            break;
        }
        case (UDT_STRING): {
            size += NexCacheModule_MallocSizeString(udt->data.str);
            break;
        }
        case (UDT_DICT): {
            void *dk;
            size_t keylen;
            NexCacheModuleString *dv;
            NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(udt->data.dict, "^", NULL, 0);
            while((dk = NexCacheModule_DictNextC(iter, &keylen, (void **)&dv)) != NULL) {
                size += keylen;
                size += NexCacheModule_MallocSizeString(dv);
            }
            NexCacheModule_DictIteratorStop(iter);
            break;
        }
    }
    
    return size;
}

/* MALLOCSIZE.SETRAW key len */
int cmd_setraw(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);
        
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);

    udt_t *udt = NexCacheModule_Alloc(sizeof(*udt));
    udt->type = UDT_RAW;
    
    long long raw_len;
    NexCacheModule_StringToLongLong(argv[2], &raw_len);
    udt->data.raw.ptr = NexCacheModule_Alloc(raw_len);
    udt->data.raw.len = raw_len;
    
    NexCacheModule_ModuleTypeSetValue(key, mallocsize_type, udt);
    NexCacheModule_CloseKey(key);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* MALLOCSIZE.SETSTR key string */
int cmd_setstr(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3)
        return NexCacheModule_WrongArity(ctx);
        
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);

    udt_t *udt = NexCacheModule_Alloc(sizeof(*udt));
    udt->type = UDT_STRING;
    
    udt->data.str = argv[2];
    NexCacheModule_RetainString(ctx, argv[2]);
    
    NexCacheModule_ModuleTypeSetValue(key, mallocsize_type, udt);
    NexCacheModule_CloseKey(key);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* MALLOCSIZE.SETDICT key field value [field value ...] */
int cmd_setdict(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 4 || argc % 2)
        return NexCacheModule_WrongArity(ctx);
        
    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_WRITE);

    udt_t *udt = NexCacheModule_Alloc(sizeof(*udt));
    udt->type = UDT_DICT;
    
    udt->data.dict = NexCacheModule_CreateDict(ctx);
    for (int i = 2; i < argc; i += 2) {
        NexCacheModule_DictSet(udt->data.dict, argv[i], argv[i+1]);
        /* No need to retain argv[i], it is copied as the rax key */
        NexCacheModule_RetainString(ctx, argv[i+1]);   
    }
    
    NexCacheModule_ModuleTypeSetValue(key, mallocsize_type, udt);
    NexCacheModule_CloseKey(key);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    if (NexCacheModule_Init(ctx,"mallocsize",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
        
    NexCacheModuleTypeMethods tm = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = udt_rdb_load,
        .rdb_save = udt_rdb_save,
        .free = udt_free,
        .mem_usage2 = udt_mem_usage,
    };

    mallocsize_type = NexCacheModule_CreateDataType(ctx, "allocsize", 0, &tm);
    if (mallocsize_type == NULL)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "mallocsize.setraw", cmd_setraw, "", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
        
    if (NexCacheModule_CreateCommand(ctx, "mallocsize.setstr", cmd_setstr, "", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
        
    if (NexCacheModule_CreateCommand(ctx, "mallocsize.setdict", cmd_setdict, "", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
