#include "nexcachemodule.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>

/* Sanity tests to verify inputs and return values. */
int sanity(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModuleRdbStream *s = NexCacheModule_RdbStreamCreateFromFile("dbnew.rdb");

    /* NULL stream should fail. */
    if (NexCacheModule_RdbLoad(ctx, NULL, 0) == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    /* Invalid flags should fail. */
    if (NexCacheModule_RdbLoad(ctx, s, 188) == NEXCACHEMODULE_OK || errno != EINVAL) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    /* Missing file should fail. */
    if (NexCacheModule_RdbLoad(ctx, s, 0) == NEXCACHEMODULE_OK || errno != ENOENT) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    /* Save RDB file. */
    if (NexCacheModule_RdbSave(ctx, s, 0) != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    /* Load the saved RDB file. */
    if (NexCacheModule_RdbLoad(ctx, s, 0) != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

 out:
    NexCacheModule_RdbStreamFree(s);
    return NEXCACHEMODULE_OK;
}

int cmd_rdbsave(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    size_t len;
    const char *filename = NexCacheModule_StringPtrLen(argv[1], &len);

    char tmp[len + 1];
    memcpy(tmp, filename, len);
    tmp[len] = '\0';

    NexCacheModuleRdbStream *stream = NexCacheModule_RdbStreamCreateFromFile(tmp);

    if (NexCacheModule_RdbSave(ctx, stream, 0) != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        goto out;
    }

    NexCacheModule_ReplyWithSimpleString(ctx, "OK");

out:
    NexCacheModule_RdbStreamFree(stream);
    return NEXCACHEMODULE_OK;
}

/* Fork before calling RM_RdbSave(). */
int cmd_rdbsave_fork(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    size_t len;
    const char *filename = NexCacheModule_StringPtrLen(argv[1], &len);

    char tmp[len + 1];
    memcpy(tmp, filename, len);
    tmp[len] = '\0';

    int fork_child_pid = NexCacheModule_Fork(NULL, NULL);
    if (fork_child_pid < 0) {
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        return NEXCACHEMODULE_OK;
    } else if (fork_child_pid > 0) {
        /* parent */
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleRdbStream *stream = NexCacheModule_RdbStreamCreateFromFile(tmp);

    int ret = 0;
    if (NexCacheModule_RdbSave(ctx, stream, 0) != NEXCACHEMODULE_OK) {
        ret = errno;
    }
    NexCacheModule_RdbStreamFree(stream);

    NexCacheModule_ExitFromChild(ret);
    return NEXCACHEMODULE_OK;
}

int cmd_rdbload(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    size_t len;
    const char *filename = NexCacheModule_StringPtrLen(argv[1], &len);

    char tmp[len + 1];
    memcpy(tmp, filename, len);
    tmp[len] = '\0';

    NexCacheModuleRdbStream *stream = NexCacheModule_RdbStreamCreateFromFile(tmp);

    if (NexCacheModule_RdbLoad(ctx, stream, 0) != NEXCACHEMODULE_OK || errno != 0) {
        NexCacheModule_RdbStreamFree(stream);
        NexCacheModule_ReplyWithError(ctx, strerror(errno));
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_RdbStreamFree(stream);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "rdbloadsave", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.sanity", sanity, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.rdbsave", cmd_rdbsave, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.rdbsave_fork", cmd_rdbsave_fork, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "test.rdbload", cmd_rdbload, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
