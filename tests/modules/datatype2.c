/* This module is used to test a use case of a module that stores information
 * about keys in global memory, and relies on the enhanced data type callbacks to
 * get key name and dbid on various operations.
 *
 * it simulates a simple memory allocator. The smallest allocation unit of 
 * the allocator is a mem block with a size of 4KB. Multiple mem blocks are combined 
 * using a linked list. These linked lists are placed in a global dict named 'mem_pool'.
 * Each db has a 'mem_pool'. You can use the 'mem.alloc' command to allocate a specified 
 * number of mem blocks, and use 'mem.free' to release the memory. Use 'mem.write', 'mem.read'
 * to write and read the specified mem block (note that each mem block can only be written once).
 * Use 'mem.usage' to get the memory usage under different dbs, and it will return the size 
 * mem blocks and used mem blocks under the db.
 * The specific structure diagram is as follows:
 * 
 * 
 * Global variables of the module:
 * 
 *                                           mem blocks link
 *                          ┌─────┬─────┐
 *                          │     │     │    ┌───┐    ┌───┐    ┌───┐
 *                          │ k1  │  ───┼───►│4KB├───►│4KB├───►│4KB│
 *                          │     │     │    └───┘    └───┘    └───┘
 *                          ├─────┼─────┤
 *    ┌───────┐      ┌────► │     │     │    ┌───┐    ┌───┐
 *    │       │      │      │ k2  │  ───┼───►│4KB├───►│4KB│
 *    │ db0   ├──────┘      │     │     │    └───┘    └───┘
 *    │       │             ├─────┼─────┤
 *    ├───────┤             │     │     │    ┌───┐    ┌───┐    ┌───┐
 *    │       │             │ k3  │  ───┼───►│4KB├───►│4KB├───►│4KB│
 *    │ db1   ├──►null      │     │     │    └───┘    └───┘    └───┘
 *    │       │             └─────┴─────┘
 *    ├───────┤                  dict
 *    │       │
 *    │ db2   ├─────────┐
 *    │       │         │
 *    ├───────┤         │   ┌─────┬─────┐
 *    │       │         │   │     │     │    ┌───┐    ┌───┐    ┌───┐
 *    │ db3   ├──►null  │   │ k1  │  ───┼───►│4KB├───►│4KB├───►│4KB│
 *    │       │         │   │     │     │    └───┘    └───┘    └───┘
 *    └───────┘         │   ├─────┼─────┤
 * mem_pool[MAX_DB]     │   │     │     │    ┌───┐    ┌───┐
 *                      └──►│ k2  │  ───┼───►│4KB├───►│4KB│
 *                          │     │     │    └───┘    └───┘
 *                          └─────┴─────┘
 *                               dict
 * 
 * 
 * Keys in server database:
 * 
 *                                ┌───────┐
 *                                │ size  │
 *                   ┌───────────►│ used  │
 *                   │            │ mask  │
 *     ┌─────┬─────┐ │            └───────┘                                   ┌───────┐
 *     │     │     │ │          MemAllocObject                                │ size  │
 *     │ k1  │  ───┼─┘                                           ┌───────────►│ used  │
 *     │     │     │                                             │            │ mask  │
 *     ├─────┼─────┤              ┌───────┐        ┌─────┬─────┐ │            └───────┘
 *     │     │     │              │ size  │        │     │     │ │          MemAllocObject
 *     │ k2  │  ───┼─────────────►│ used  │        │ k1  │  ───┼─┘
 *     │     │     │              │ mask  │        │     │     │
 *     ├─────┼─────┤              └───────┘        ├─────┼─────┤
 *     │     │     │            MemAllocObject     │     │     │
 *     │ k3  │  ───┼─┐                             │ k2  │  ───┼─┐
 *     │     │     │ │                             │     │     │ │
 *     └─────┴─────┘ │            ┌───────┐        └─────┴─────┘ │            ┌───────┐
 *     server db[0]  │            │ size  │         server db[1] │            │ size  │
 *                   └───────────►│ used  │                      └───────────►│ used  │
 *                                │ mask  │                                   │ mask  │
 *                                └───────┘                                   └───────┘
 *                              MemAllocObject                              MemAllocObject
 *
 **/

#include "nexcachemodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

static NexCacheModuleType *MemAllocType;

#define MAX_DB 16
NexCacheModuleDict *mem_pool[MAX_DB];
typedef struct MemAllocObject {
    long long size;
    long long used;
    uint64_t mask;
} MemAllocObject;

MemAllocObject *createMemAllocObject(void) {
    MemAllocObject *o = NexCacheModule_Calloc(1, sizeof(*o));
    return o;
}

/*---------------------------- mem block apis ------------------------------------*/
#define BLOCK_SIZE 4096
struct MemBlock {
    char block[BLOCK_SIZE];
    struct MemBlock *next;
};

void MemBlockFree(struct MemBlock *head) {
    if (head) {
        struct MemBlock *block = head->next, *next;
        NexCacheModule_Free(head);
        while (block) {
            next = block->next;
            NexCacheModule_Free(block);
            block = next;
        }
    }
}
struct MemBlock *MemBlockCreate(long long num) {
    if (num <= 0) {
        return NULL;
    }

    struct MemBlock *head = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
    struct MemBlock *block = head;
    while (--num) {
        block->next = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
        block = block->next;
    }

    return head;
}

long long MemBlockNum(const struct MemBlock *head) {
    long long num = 0;
    const struct MemBlock *block = head;
    while (block) {
        num++;
        block = block->next;
    }

    return num;
}

size_t MemBlockWrite(struct MemBlock *head, long long block_index, const char *data, size_t size) {
    size_t w_size = 0;
    struct MemBlock *block = head;
    while (block_index-- && block) {
        block = block->next;
    }

    if (block) {
        size = size > BLOCK_SIZE ? BLOCK_SIZE:size;
        memcpy(block->block, data, size);
        w_size += size;
    }

    return w_size;
}

int MemBlockRead(struct MemBlock *head, long long block_index, char *data, size_t size) {
    size_t r_size = 0;
    struct MemBlock *block = head;
    while (block_index-- && block) {
        block = block->next;
    }

    if (block) {
        size = size > BLOCK_SIZE ? BLOCK_SIZE:size;
        memcpy(data, block->block, size);
        r_size += size;
    }

    return r_size;
}

void MemPoolFreeDb(NexCacheModuleCtx *ctx, int dbid) {
    NexCacheModuleString *key;
    void *tdata;
    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(mem_pool[dbid], "^", NULL, 0);
    while((key = NexCacheModule_DictNext(ctx, iter, &tdata)) != NULL) {
        MemBlockFree((struct MemBlock *)tdata);
    }
    NexCacheModule_DictIteratorStop(iter);
    NexCacheModule_FreeDict(NULL, mem_pool[dbid]);
    mem_pool[dbid] = NexCacheModule_CreateDict(NULL);
}

struct MemBlock *MemBlockClone(const struct MemBlock *head) {
    struct MemBlock *newhead = NULL;
    if (head) {
        newhead = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
        memcpy(newhead->block, head->block, BLOCK_SIZE);
        struct MemBlock *newblock = newhead;
        const struct MemBlock *oldblock = head->next;
        while (oldblock) {
            newblock->next = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
            newblock = newblock->next;
            memcpy(newblock->block, oldblock->block, BLOCK_SIZE);
            oldblock = oldblock->next;
        }
    }

    return newhead;
}

/*---------------------------- event handler ------------------------------------*/
void swapDbCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(e);
    NEXCACHEMODULE_NOT_USED(sub);

    NexCacheModuleSwapDbInfo *ei = data;

    // swap
    NexCacheModuleDict *tmp = mem_pool[ei->dbnum_first];
    mem_pool[ei->dbnum_first] = mem_pool[ei->dbnum_second];
    mem_pool[ei->dbnum_second] = tmp;
}

void flushdbCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(e);
    int i;
    NexCacheModuleFlushInfo *fi = data;

    NexCacheModule_AutoMemory(ctx);

    if (sub == NEXCACHEMODULE_SUBEVENT_FLUSHDB_START) {
        if (fi->dbnum != -1) {
           MemPoolFreeDb(ctx, fi->dbnum);
        } else {
            for (i = 0; i < MAX_DB; i++) {
                MemPoolFreeDb(ctx, i);
            }
        }
    }
}

/*---------------------------- command implementation ------------------------------------*/

/* MEM.ALLOC key block_num */
int MemAlloc_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc != 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long block_num;
    if ((NexCacheModule_StringToLongLong(argv[2], &block_num) != NEXCACHEMODULE_OK) || block_num <= 0) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid block_num: must be a value greater than 0");
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != MemAllocType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    MemAllocObject *o;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        o = createMemAllocObject();
        NexCacheModule_ModuleTypeSetValue(key, MemAllocType, o);
    } else {
        o = NexCacheModule_ModuleTypeGetValue(key);
    }

    struct MemBlock *mem = MemBlockCreate(block_num);
    NexCacheModule_Assert(mem != NULL);
    NexCacheModule_DictSet(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], mem);
    o->size = block_num;
    o->used = 0;
    o->mask = 0;

    NexCacheModule_ReplyWithLongLong(ctx, block_num);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* MEM.FREE key */
int MemFree_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc != 2) {
        return NexCacheModule_WrongArity(ctx);
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != MemAllocType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    int ret = 0;
    MemAllocObject *o;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        NexCacheModule_ReplyWithLongLong(ctx, ret);
        return NEXCACHEMODULE_OK;
    } else {
        o = NexCacheModule_ModuleTypeGetValue(key);
    }

    int nokey;
    struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], &nokey);
    if (!nokey && mem) {
        NexCacheModule_DictDel(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], NULL);
        MemBlockFree(mem);
        o->used = 0;
        o->size = 0;
        o->mask = 0;
        ret = 1;
    }

    NexCacheModule_ReplyWithLongLong(ctx, ret);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* MEM.WRITE key block_index data */
int MemWrite_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc != 4) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long block_index;
    if ((NexCacheModule_StringToLongLong(argv[2], &block_index) != NEXCACHEMODULE_OK) || block_index < 0) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid block_index: must be a value greater than 0");
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != MemAllocType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    MemAllocObject *o;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        return NexCacheModule_ReplyWithError(ctx, "ERR Memory has not been allocated");
    } else {
        o = NexCacheModule_ModuleTypeGetValue(key);
    }

    if (o->mask & (1UL << block_index)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR block is busy");
    }

    int ret = 0;
    int nokey;
    struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], &nokey);
    if (!nokey && mem) {
        size_t len;
        const char *buf = NexCacheModule_StringPtrLen(argv[3], &len);
        ret = MemBlockWrite(mem, block_index, buf, len);
        o->mask |= (1UL << block_index);
        o->used++;
    }

    NexCacheModule_ReplyWithLongLong(ctx, ret);
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/* MEM.READ key block_index */
int MemRead_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc != 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long block_index;
    if ((NexCacheModule_StringToLongLong(argv[2], &block_index) != NEXCACHEMODULE_OK) || block_index < 0) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid block_index: must be a value greater than 0");
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != MemAllocType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    MemAllocObject *o;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        return NexCacheModule_ReplyWithError(ctx, "ERR Memory has not been allocated");
    } else {
        o = NexCacheModule_ModuleTypeGetValue(key);
    }

    if (!(o->mask & (1UL << block_index))) {
        return NexCacheModule_ReplyWithNull(ctx);
    }

    int nokey;
    struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], &nokey);
    NexCacheModule_Assert(nokey == 0 && mem != NULL);
     
    char buf[BLOCK_SIZE];
    MemBlockRead(mem, block_index, buf, sizeof(buf));
    
    /* Assuming that the contents are all c-style strings */
    NexCacheModule_ReplyWithStringBuffer(ctx, buf, strlen(buf));
    return NEXCACHEMODULE_OK;
}

/* MEM.USAGE dbid */
int MemUsage_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc != 2) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long dbid;
    if ((NexCacheModule_StringToLongLong(argv[1], (long long *)&dbid) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid value: must be a integer");
    }

    if (dbid < 0 || dbid >= MAX_DB) {
        return NexCacheModule_ReplyWithError(ctx, "ERR dbid out of range");
    }


    long long size = 0, used = 0;

    void *data;
    NexCacheModuleString *key;
    NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(mem_pool[dbid], "^", NULL, 0);
    while((key = NexCacheModule_DictNext(ctx, iter, &data)) != NULL) {
        int dbbackup = NexCacheModule_GetSelectedDb(ctx);
        NexCacheModule_SelectDb(ctx, dbid);
        NexCacheModuleKey *openkey = NexCacheModule_OpenKey(ctx, key, NEXCACHEMODULE_READ);
        int type = NexCacheModule_KeyType(openkey);
        NexCacheModule_Assert(type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(openkey) == MemAllocType);
        MemAllocObject *o = NexCacheModule_ModuleTypeGetValue(openkey);
        used += o->used;
        size += o->size;
        NexCacheModule_CloseKey(openkey);
        NexCacheModule_SelectDb(ctx, dbbackup);
    }
    NexCacheModule_DictIteratorStop(iter);

    NexCacheModule_ReplyWithArray(ctx, 4);
    NexCacheModule_ReplyWithSimpleString(ctx, "total");
    NexCacheModule_ReplyWithLongLong(ctx, size);
    NexCacheModule_ReplyWithSimpleString(ctx, "used");
    NexCacheModule_ReplyWithLongLong(ctx, used);
    return NEXCACHEMODULE_OK;
}

/* MEM.ALLOCANDWRITE key block_num block_index data block_index data ... */
int MemAllocAndWrite_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModule_AutoMemory(ctx);  

    if (argc < 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long block_num;
    if ((NexCacheModule_StringToLongLong(argv[2], &block_num) != NEXCACHEMODULE_OK) || block_num <= 0) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid block_num: must be a value greater than 0");
    }

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx, argv[1], NEXCACHEMODULE_READ | NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY && NexCacheModule_ModuleTypeGetType(key) != MemAllocType) {
        return NexCacheModule_ReplyWithError(ctx, NEXCACHEMODULE_ERRORMSG_WRONGTYPE);
    }

    MemAllocObject *o;
    if (type == NEXCACHEMODULE_KEYTYPE_EMPTY) {
        o = createMemAllocObject();
        NexCacheModule_ModuleTypeSetValue(key, MemAllocType, o);
    } else {
        o = NexCacheModule_ModuleTypeGetValue(key);
    }

    struct MemBlock *mem = MemBlockCreate(block_num);
    NexCacheModule_Assert(mem != NULL);
    NexCacheModule_DictSet(mem_pool[NexCacheModule_GetSelectedDb(ctx)], argv[1], mem);
    o->used = 0;
    o->mask = 0;
    o->size = block_num;

    int i = 3;
    long long block_index;
    for (; i < argc; i++) {
        /* Security is guaranteed internally, so no security check. */
        NexCacheModule_StringToLongLong(argv[i], &block_index);
        size_t len;
        const char * buf = NexCacheModule_StringPtrLen(argv[i + 1], &len);
        MemBlockWrite(mem, block_index, buf, len);
        o->used++;
        o->mask |= (1UL << block_index);
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    NexCacheModule_ReplicateVerbatim(ctx);
    return NEXCACHEMODULE_OK;
}

/*---------------------------- type callbacks ------------------------------------*/

void *MemAllocRdbLoad(NexCacheModuleIO *rdb, int encver) {
    if (encver != 0) {
        return NULL;
    }

    MemAllocObject *o = createMemAllocObject();
    o->size = NexCacheModule_LoadSigned(rdb);
    o->used = NexCacheModule_LoadSigned(rdb);
    o->mask = NexCacheModule_LoadUnsigned(rdb);

    const NexCacheModuleString *key = NexCacheModule_GetKeyNameFromIO(rdb);
    int dbid = NexCacheModule_GetDbIdFromIO(rdb);

    if (o->size) {
        size_t size;
        char *tmpbuf;
        long long num = o->size;
        struct MemBlock *head = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
        tmpbuf = NexCacheModule_LoadStringBuffer(rdb, &size);
        memcpy(head->block, tmpbuf, size > BLOCK_SIZE ? BLOCK_SIZE:size);
        NexCacheModule_Free(tmpbuf);
        struct MemBlock *block = head;
        while (--num) {
            block->next = NexCacheModule_Calloc(1, sizeof(struct MemBlock));
            block = block->next;

            tmpbuf = NexCacheModule_LoadStringBuffer(rdb, &size);
            memcpy(block->block, tmpbuf, size > BLOCK_SIZE ? BLOCK_SIZE:size);
            NexCacheModule_Free(tmpbuf);
        }

        NexCacheModule_DictSet(mem_pool[dbid], (NexCacheModuleString *)key, head);
    }
     
    return o;
}

void MemAllocRdbSave(NexCacheModuleIO *rdb, void *value) {
    MemAllocObject *o = value;
    NexCacheModule_SaveSigned(rdb, o->size);
    NexCacheModule_SaveSigned(rdb, o->used);
    NexCacheModule_SaveUnsigned(rdb, o->mask);

    const NexCacheModuleString *key = NexCacheModule_GetKeyNameFromIO(rdb);
    int dbid = NexCacheModule_GetDbIdFromIO(rdb);

    if (o->size) {
        int nokey;
        struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[dbid], (NexCacheModuleString *)key, &nokey);
        NexCacheModule_Assert(nokey == 0 && mem != NULL);

        struct MemBlock *block = mem; 
        while (block) {
            NexCacheModule_SaveStringBuffer(rdb, block->block, BLOCK_SIZE);
            block = block->next;
        }
    }
}

void MemAllocAofRewrite(NexCacheModuleIO *aof, NexCacheModuleString *key, void *value) {
    MemAllocObject *o = (MemAllocObject *)value;
    if (o->size) {
        int dbid = NexCacheModule_GetDbIdFromIO(aof);
        int nokey;
        size_t i = 0, j = 0;
        struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[dbid], (NexCacheModuleString *)key, &nokey);
        NexCacheModule_Assert(nokey == 0 && mem != NULL);
        size_t array_size = o->size * 2;
        NexCacheModuleString ** string_array = NexCacheModule_Calloc(array_size, sizeof(NexCacheModuleString *));
        while (mem) {
            string_array[i] = NexCacheModule_CreateStringFromLongLong(NULL, j);
            string_array[i + 1] = NexCacheModule_CreateString(NULL, mem->block, BLOCK_SIZE);
            mem = mem->next;
            i += 2;
            j++;
        }
        NexCacheModule_EmitAOF(aof, "mem.allocandwrite", "slv", key, o->size, string_array, array_size);
        for (i = 0; i < array_size; i++) {
            NexCacheModule_FreeString(NULL, string_array[i]);
        }
        NexCacheModule_Free(string_array);
    } else {
        NexCacheModule_EmitAOF(aof, "mem.allocandwrite", "sl", key, o->size);
    }
}

void MemAllocFree(void *value) {
    NexCacheModule_Free(value);
}

void MemAllocUnlink(NexCacheModuleString *key, const void *value) {
    NEXCACHEMODULE_NOT_USED(key);
    NEXCACHEMODULE_NOT_USED(value);

    /* When unlink and unlink2 exist at the same time, we will only call unlink2. */
    NexCacheModule_Assert(0);
}

void MemAllocUnlink2(NexCacheModuleKeyOptCtx *ctx, const void *value) {
    MemAllocObject *o = (MemAllocObject *)value;

    const NexCacheModuleString *key = NexCacheModule_GetKeyNameFromOptCtx(ctx);
    int dbid = NexCacheModule_GetDbIdFromOptCtx(ctx);
    
    if (o->size) {
        void *oldval;
        NexCacheModule_DictDel(mem_pool[dbid], (NexCacheModuleString *)key, &oldval);
        NexCacheModule_Assert(oldval != NULL);
        MemBlockFree((struct MemBlock *)oldval);
    }
}

void MemAllocDigest(NexCacheModuleDigest *md, void *value) {
    MemAllocObject *o = (MemAllocObject *)value;
    NexCacheModule_DigestAddLongLong(md, o->size);
    NexCacheModule_DigestAddLongLong(md, o->used);
    NexCacheModule_DigestAddLongLong(md, o->mask);

    int dbid = NexCacheModule_GetDbIdFromDigest(md);
    const NexCacheModuleString *key = NexCacheModule_GetKeyNameFromDigest(md);
    
    if (o->size) {
        int nokey;
        struct MemBlock *mem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[dbid], (NexCacheModuleString *)key, &nokey);
        NexCacheModule_Assert(nokey == 0 && mem != NULL);

        struct MemBlock *block = mem;
        while (block) {
            NexCacheModule_DigestAddStringBuffer(md, (const char *)block->block, BLOCK_SIZE);
            block = block->next;
        }
    }
}

void *MemAllocCopy2(NexCacheModuleKeyOptCtx *ctx, const void *value) {
    const MemAllocObject *old = value;
    MemAllocObject *new = createMemAllocObject();
    new->size = old->size;
    new->used = old->used;
    new->mask = old->mask;

    int from_dbid = NexCacheModule_GetDbIdFromOptCtx(ctx);
    int to_dbid = NexCacheModule_GetToDbIdFromOptCtx(ctx);
    const NexCacheModuleString *fromkey = NexCacheModule_GetKeyNameFromOptCtx(ctx);
    const NexCacheModuleString *tokey = NexCacheModule_GetToKeyNameFromOptCtx(ctx);

    if (old->size) {
        int nokey;
        struct MemBlock *oldmem = (struct MemBlock *)NexCacheModule_DictGet(mem_pool[from_dbid], (NexCacheModuleString *)fromkey, &nokey);
        NexCacheModule_Assert(nokey == 0 && oldmem != NULL);
        struct MemBlock *newmem = MemBlockClone(oldmem);
        NexCacheModule_Assert(newmem != NULL);
        NexCacheModule_DictSet(mem_pool[to_dbid], (NexCacheModuleString *)tokey, newmem);
    }   

    return new;
}

size_t MemAllocMemUsage2(NexCacheModuleKeyOptCtx *ctx, const void *value, size_t sample_size) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(sample_size);
    uint64_t size = 0;
    MemAllocObject *o = (MemAllocObject *)value;

    size += sizeof(*o);
    size += o->size * sizeof(struct MemBlock);

    return size;
}

size_t MemAllocMemFreeEffort2(NexCacheModuleKeyOptCtx *ctx, const void *value) {
    NEXCACHEMODULE_NOT_USED(ctx);
    MemAllocObject *o = (MemAllocObject *)value;
    return o->size;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "datatype2", 1,NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    NexCacheModuleTypeMethods tm = {
        .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
        .rdb_load = MemAllocRdbLoad,
        .rdb_save = MemAllocRdbSave,
        .aof_rewrite = MemAllocAofRewrite,
        .free = MemAllocFree,
        .digest = MemAllocDigest,
        .unlink = MemAllocUnlink,
        // .defrag = MemAllocDefrag, // Tested in defragtest.c
        .unlink2 = MemAllocUnlink2,
        .copy2 = MemAllocCopy2,
        .mem_usage2 = MemAllocMemUsage2,
        .free_effort2 = MemAllocMemFreeEffort2,
    };

    MemAllocType = NexCacheModule_CreateDataType(ctx, "mem_alloc", 0, &tm);
    if (MemAllocType == NULL) {
        return NEXCACHEMODULE_ERR;
    }

    /* This option tests skip command validation for NexCacheModule_EmitAOF */
    NexCacheModule_SetModuleOptions(ctx, NEXCACHEMODULE_OPTIONS_SKIP_COMMAND_VALIDATION);

    if (NexCacheModule_CreateCommand(ctx, "mem.alloc", MemAlloc_NexCacheCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "mem.free", MemFree_NexCacheCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "mem.write", MemWrite_NexCacheCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "mem.read", MemRead_NexCacheCommand, "readonly", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    if (NexCacheModule_CreateCommand(ctx, "mem.usage", MemUsage_NexCacheCommand, "readonly", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    /* used for internal aof rewrite */
    if (NexCacheModule_CreateCommand(ctx, "mem.allocandwrite", MemAllocAndWrite_NexCacheCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    for(int i = 0; i < MAX_DB; i++){
        mem_pool[i] = NexCacheModule_CreateDict(NULL);
    }

    NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_FlushDB, flushdbCallback);
    NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_SwapDB, swapDbCallback);

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    NEXCACHEMODULE_NOT_USED(ctx);

    for(int i = 0; i < MAX_DB; i++){
        NexCacheModuleString *key;
        void *tdata;
        NexCacheModuleDictIter *iter = NexCacheModule_DictIteratorStartC(mem_pool[i], "^", NULL, 0);
        while((key = NexCacheModule_DictNext(ctx, iter, &tdata)) != NULL) {
            MemBlockFree((struct MemBlock *)tdata);
            NexCacheModule_FreeString(ctx, key);
        }
        NexCacheModule_DictIteratorStop(iter);
        NexCacheModule_FreeDict(NULL, mem_pool[i]);
    }

    return NEXCACHEMODULE_OK;
}