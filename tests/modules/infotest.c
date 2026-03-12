#include "nexcachemodule.h"

#include <string.h>

void InfoFunc(NexCacheModuleInfoCtx *ctx, int for_crash_report) {
    NexCacheModule_InfoAddSection(ctx, "");
    NexCacheModule_InfoAddFieldLongLong(ctx, "global", -2);
    NexCacheModule_InfoAddFieldULongLong(ctx, "uglobal", (unsigned long long)-2);

    NexCacheModule_InfoAddSection(ctx, "Spanish");
    NexCacheModule_InfoAddFieldCString(ctx, "uno", "one");
    NexCacheModule_InfoAddFieldLongLong(ctx, "dos", 2);

    NexCacheModule_InfoAddSection(ctx, "Italian");
    NexCacheModule_InfoAddFieldLongLong(ctx, "due", 2);
    NexCacheModule_InfoAddFieldDouble(ctx, "tre", 3.3);

    NexCacheModule_InfoAddSection(ctx, "keyspace");
    NexCacheModule_InfoBeginDictField(ctx, "db0");
    NexCacheModule_InfoAddFieldLongLong(ctx, "keys", 3);
    NexCacheModule_InfoAddFieldLongLong(ctx, "expires", 1);
    NexCacheModule_InfoEndDictField(ctx);

    NexCacheModule_InfoAddSection(ctx, "unsafe");
    NexCacheModule_InfoBeginDictField(ctx, "unsafe:field");
    NexCacheModule_InfoAddFieldLongLong(ctx, "value", 1);
    NexCacheModule_InfoEndDictField(ctx);

    if (for_crash_report) {
        NexCacheModule_InfoAddSection(ctx, "Klingon");
        NexCacheModule_InfoAddFieldCString(ctx, "one", "wa'");
        NexCacheModule_InfoAddFieldCString(ctx, "two", "cha'");
        NexCacheModule_InfoAddFieldCString(ctx, "three", "wej");
    }

}

int info_get(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc, char field_type)
{
    if (argc != 3 && argc != 4) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }
    int err = NEXCACHEMODULE_OK;
    const char *section, *field;
    section = NexCacheModule_StringPtrLen(argv[1], NULL);
    field = NexCacheModule_StringPtrLen(argv[2], NULL);
    NexCacheModuleServerInfoData *info = NexCacheModule_GetServerInfo(ctx, section);
    if (field_type=='i') {
        long long ll = NexCacheModule_ServerInfoGetFieldSigned(info, field, &err);
        if (err==NEXCACHEMODULE_OK)
            NexCacheModule_ReplyWithLongLong(ctx, ll);
    } else if (field_type=='u') {
        unsigned long long ll = (unsigned long long)NexCacheModule_ServerInfoGetFieldUnsigned(info, field, &err);
        if (err==NEXCACHEMODULE_OK)
            NexCacheModule_ReplyWithLongLong(ctx, ll);
    } else if (field_type=='d') {
        double d = NexCacheModule_ServerInfoGetFieldDouble(info, field, &err);
        if (err==NEXCACHEMODULE_OK)
            NexCacheModule_ReplyWithDouble(ctx, d);
    } else if (field_type=='c') {
        const char *str = NexCacheModule_ServerInfoGetFieldC(info, field);
        if (str)
            NexCacheModule_ReplyWithCString(ctx, str);
    } else {
        NexCacheModuleString *str = NexCacheModule_ServerInfoGetField(ctx, info, field);
        if (str) {
            NexCacheModule_ReplyWithString(ctx, str);
            NexCacheModule_FreeString(ctx, str);
        } else
            err=NEXCACHEMODULE_ERR;
    }
    if (err!=NEXCACHEMODULE_OK)
        NexCacheModule_ReplyWithError(ctx, "not found");
    NexCacheModule_FreeServerInfo(ctx, info);
    return NEXCACHEMODULE_OK;
}

int info_gets(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    return info_get(ctx, argv, argc, 's');
}

int info_getc(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    return info_get(ctx, argv, argc, 'c');
}

int info_geti(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    return info_get(ctx, argv, argc, 'i');
}

int info_getu(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    return info_get(ctx, argv, argc, 'u');
}

int info_getd(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    return info_get(ctx, argv, argc, 'd');
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"infotest",1,NEXCACHEMODULE_APIVER_1)
            == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_RegisterInfoFunc(ctx, InfoFunc) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"info.gets", info_gets,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"info.getc", info_getc,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"info.geti", info_geti,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"info.getu", info_getu,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"info.getd", info_getd,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
