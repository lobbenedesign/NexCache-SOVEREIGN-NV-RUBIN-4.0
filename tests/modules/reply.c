/* 
 * A module the tests RM_ReplyWith family of commands
 */

#include "nexcachemodule.h"
#include <math.h>

int rw_string(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    return NexCacheModule_ReplyWithString(ctx, argv[1]);
}

int rw_cstring(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    return NexCacheModule_ReplyWithSimpleString(ctx, "A simple string");
}

int rw_int(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long long integer;
    if (NexCacheModule_StringToLongLong(argv[1], &integer) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as an integer");

    return NexCacheModule_ReplyWithLongLong(ctx, integer);
}

/* When one argument is given, it is returned as a double,
 * when two arguments are given, it returns a/b. */
int rw_double(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc==1)
        return NexCacheModule_ReplyWithDouble(ctx, NAN);

    if (argc != 2 && argc != 3) return NexCacheModule_WrongArity(ctx);

    double dbl, dbl2;
    if (NexCacheModule_StringToDouble(argv[1], &dbl) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a double");
    if (argc == 3) {
        if (NexCacheModule_StringToDouble(argv[2], &dbl2) != NEXCACHEMODULE_OK)
            return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a double");
        dbl /= dbl2;
    }

    return NexCacheModule_ReplyWithDouble(ctx, dbl);
}

int rw_longdouble(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long double longdbl;
    if (NexCacheModule_StringToLongDouble(argv[1], &longdbl) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a double");

    return NexCacheModule_ReplyWithLongDouble(ctx, longdbl);
}

int rw_bignumber(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    size_t bignum_len;
    const char *bignum_str = NexCacheModule_StringPtrLen(argv[1], &bignum_len);

    return NexCacheModule_ReplyWithBigNumber(ctx, bignum_str, bignum_len);
}

int rw_array(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long long integer;
    if (NexCacheModule_StringToLongLong(argv[1], &integer) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a integer");

    NexCacheModule_ReplyWithArray(ctx, integer);
    for (int i = 0; i < integer; ++i) {
        NexCacheModule_ReplyWithLongLong(ctx, i);
    }

    return NEXCACHEMODULE_OK;
}

int rw_map(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long long integer;
    if (NexCacheModule_StringToLongLong(argv[1], &integer) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a integer");

    NexCacheModule_ReplyWithMap(ctx, integer);
    for (int i = 0; i < integer; ++i) {
        NexCacheModule_ReplyWithLongLong(ctx, i);
        NexCacheModule_ReplyWithDouble(ctx, i * 1.5);
    }

    return NEXCACHEMODULE_OK;
}

int rw_set(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long long integer;
    if (NexCacheModule_StringToLongLong(argv[1], &integer) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a integer");

    NexCacheModule_ReplyWithSet(ctx, integer);
    for (int i = 0; i < integer; ++i) {
        NexCacheModule_ReplyWithLongLong(ctx, i);
    }

    return NEXCACHEMODULE_OK;
}

int rw_attribute(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    long long integer;
    if (NexCacheModule_StringToLongLong(argv[1], &integer) != NEXCACHEMODULE_OK)
        return NexCacheModule_ReplyWithError(ctx, "Arg cannot be parsed as a integer");

    if (NexCacheModule_ReplyWithAttribute(ctx, integer) != NEXCACHEMODULE_OK) {
        return NexCacheModule_ReplyWithError(ctx, "Attributes aren't supported by RESP 2");
    }

    for (int i = 0; i < integer; ++i) {
        NexCacheModule_ReplyWithLongLong(ctx, i);
        NexCacheModule_ReplyWithDouble(ctx, i * 1.5);
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int rw_bool(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    NexCacheModule_ReplyWithArray(ctx, 2);
    NexCacheModule_ReplyWithBool(ctx, 0);
    return NexCacheModule_ReplyWithBool(ctx, 1);
}

int rw_null(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    return NexCacheModule_ReplyWithNull(ctx);
}

int rw_error(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    if (argc != 1) return NexCacheModule_WrongArity(ctx);

    return NexCacheModule_ReplyWithError(ctx, "An error");
}

int rw_error_format(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) return NexCacheModule_WrongArity(ctx);

    return NexCacheModule_ReplyWithErrorFormat(ctx,
                                            NexCacheModule_StringPtrLen(argv[1], NULL),
                                            NexCacheModule_StringPtrLen(argv[2], NULL));
}

int rw_verbatim(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    size_t verbatim_len;
    const char *verbatim_str = NexCacheModule_StringPtrLen(argv[1], &verbatim_len);

    return NexCacheModule_ReplyWithVerbatimString(ctx, verbatim_str, verbatim_len);
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx, "replywith", 1, NEXCACHEMODULE_APIVER_1) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"rw.string",rw_string,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.cstring",rw_cstring,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.bignumber",rw_bignumber,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.int",rw_int,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.double",rw_double,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.longdouble",rw_longdouble,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.array",rw_array,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.map",rw_map,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.attribute",rw_attribute,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.set",rw_set,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.bool",rw_bool,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.null",rw_null,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.error",rw_error,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.error_format",rw_error_format,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"rw.verbatim",rw_verbatim,"",0,0,0) != NEXCACHEMODULE_OK)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
