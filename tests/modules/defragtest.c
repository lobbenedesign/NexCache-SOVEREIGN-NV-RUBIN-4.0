/* A module that implements defrag callback mechanisms.
 */

#include "nexcachemodule.h"
#include <stdlib.h>

static NexCacheModuleType *FragType;

struct FragObject {
    unsigned long len;
    void **values;
    int maxstep;
};

/* Make sure we get the expected cursor */
unsigned long int last_set_cursor = 0;

unsigned long int datatype_attempts = 0;
unsigned long int datatype_defragged = 0;
unsigned long int datatype_resumes = 0;
unsigned long int datatype_wrong_cursor = 0;
unsigned long int global_attempts = 0;
unsigned long int global_defragged = 0;

int global_strings_len = 0;
NexCacheModuleString **global_strings = NULL;

static void createGlobalStrings(NexCacheModuleCtx *ctx, int count)
{
    global_strings_len = count;
    global_strings = NexCacheModule_Alloc(sizeof(NexCacheModuleString *) * count);

    for (int i = 0; i < count; i++) {
        global_strings[i] = NexCacheModule_CreateStringFromLongLong(ctx, i);
    }
}

static void defragGlobalStrings(NexCacheModuleDefragCtx *ctx)
{
    for (int i = 0; i < global_strings_len; i++) {
        NexCacheModuleString *new = NexCacheModule_DefragNexCacheModuleString(ctx, global_strings[i]);
        global_attempts++;
        if (new != NULL) {
            global_strings[i] = new;
            global_defragged++;
        }
    }
}

static void FragInfo(NexCacheModuleInfoCtx *ctx, int for_crash_report) {
    NEXCACHEMODULE_NOT_USED(for_crash_report);

    NexCacheModule_InfoAddSection(ctx, "stats");
    NexCacheModule_InfoAddFieldLongLong(ctx, "datatype_attempts", datatype_attempts);
    NexCacheModule_InfoAddFieldLongLong(ctx, "datatype_defragged", datatype_defragged);
    NexCacheModule_InfoAddFieldLongLong(ctx, "datatype_resumes", datatype_resumes);
    NexCacheModule_InfoAddFieldLongLong(ctx, "datatype_wrong_cursor", datatype_wrong_cursor);
    NexCacheModule_InfoAddFieldLongLong(ctx, "global_attempts", global_attempts);
    NexCacheModule_InfoAddFieldLongLong(ctx, "global_defragged", global_defragged);
}

struct FragObject *createFragObject(unsigned long len, unsigned long size, int maxstep) {
    struct FragObject *o = NexCacheModule_Alloc(sizeof(*o));
    o->len = len;
    o->values = NexCacheModule_Alloc(sizeof(NexCacheModuleString*) * len);
    o->maxstep = maxstep;

    for (unsigned long i = 0; i < len; i++) {
        o->values[i] = NexCacheModule_Calloc(1, size);
    }

    return o;
}

/* FRAG.RESETSTATS */
static int fragResetStatsCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    datatype_attempts = 0;
    datatype_defragged = 0;
    datatype_resumes = 0;
    datatype_wrong_cursor = 0;
    global_attempts = 0;
    global_defragged = 0;

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* FRAG.CREATE key len size maxstep */
static int fragCreateCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 5)
        return NexCacheModule_WrongArity(ctx);

    NexCacheModuleKey *key = NexCacheModule_OpenKey(ctx,argv[1],
                                              NEXCACHEMODULE_READ|NEXCACHEMODULE_WRITE);
    int type = NexCacheModule_KeyType(key);
    if (type != NEXCACHEMODULE_KEYTYPE_EMPTY)
    {
        return NexCacheModule_ReplyWithError(ctx, "ERR key exists");
    }

    long long len;
    if ((NexCacheModule_StringToLongLong(argv[2], &len) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid len");
    }

    long long size;
    if ((NexCacheModule_StringToLongLong(argv[3], &size) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid size");
    }

    long long maxstep;
    if ((NexCacheModule_StringToLongLong(argv[4], &maxstep) != NEXCACHEMODULE_OK)) {
        return NexCacheModule_ReplyWithError(ctx, "ERR invalid maxstep");
    }

    struct FragObject *o = createFragObject(len, size, maxstep);
    NexCacheModule_ModuleTypeSetValue(key, FragType, o);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    NexCacheModule_CloseKey(key);

    return NEXCACHEMODULE_OK;
}

void FragFree(void *value) {
    struct FragObject *o = value;

    for (unsigned long i = 0; i < o->len; i++)
        NexCacheModule_Free(o->values[i]);
    NexCacheModule_Free(o->values);
    NexCacheModule_Free(o);
}

size_t FragFreeEffort(NexCacheModuleString *key, const void *value) {
    NEXCACHEMODULE_NOT_USED(key);

    const struct FragObject *o = value;
    return o->len;
}

int FragDefrag(NexCacheModuleDefragCtx *ctx, NexCacheModuleString *key, void **value) {
    NEXCACHEMODULE_NOT_USED(key);
    unsigned long i = 0;
    int steps = 0;

    int dbid = NexCacheModule_GetDbIdFromDefragCtx(ctx);
    NexCacheModule_Assert(dbid != -1);

    /* Attempt to get cursor, validate it's what we're exepcting */
    if (NexCacheModule_DefragCursorGet(ctx, &i) == NEXCACHEMODULE_OK) {
        if (i > 0) datatype_resumes++;

        /* Validate we're expecting this cursor */
        if (i != last_set_cursor) datatype_wrong_cursor++;
    } else {
        if (last_set_cursor != 0) datatype_wrong_cursor++;
    }

    /* Attempt to defrag the object itself */
    datatype_attempts++;
    struct FragObject *o = NexCacheModule_DefragAlloc(ctx, *value);
    if (o == NULL) {
        /* Not defragged */
        o = *value;
    } else {
        /* Defragged */
        *value = o;
        datatype_defragged++;
    }

    /* Deep defrag now */
    for (; i < o->len; i++) {
        datatype_attempts++;
        void *new = NexCacheModule_DefragAlloc(ctx, o->values[i]);
        if (new) {
            o->values[i] = new;
            datatype_defragged++;
        }

        if ((o->maxstep && ++steps > o->maxstep) ||
            ((i % 64 == 0) && NexCacheModule_DefragShouldStop(ctx)))
        {
            NexCacheModule_DefragCursorSet(ctx, i);
            last_set_cursor = i;
            return 1;
        }
    }

    last_set_cursor = 0;
    return 0;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "defragtest", 1, NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_GetTypeMethodVersion() < NEXCACHEMODULE_TYPE_METHOD_VERSION) {
        return NEXCACHEMODULE_ERR;
    }

    long long glen;
    if (argc != 1 || NexCacheModule_StringToLongLong(argv[0], &glen) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    }

    createGlobalStrings(ctx, glen);

    NexCacheModuleTypeMethods tm = {
            .version = NEXCACHEMODULE_TYPE_METHOD_VERSION,
            .free = FragFree,
            .free_effort = FragFreeEffort,
            .defrag = FragDefrag
    };

    FragType = NexCacheModule_CreateDataType(ctx, "frag_type", 0, &tm);
    if (FragType == NULL) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "frag.create",
                                  fragCreateCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "frag.resetstats",
                                  fragResetStatsCommand, "write deny-oom", 1, 1, 1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    NexCacheModule_RegisterInfoFunc(ctx, FragInfo);
    NexCacheModule_RegisterDefragFunc(ctx, defragGlobalStrings);

    return NEXCACHEMODULE_OK;
}
