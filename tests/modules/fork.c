
/* define macros for having usleep */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include "nexcachemodule.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define UNUSED(V) ((void) V)

int child_pid = -1;
int exited_with_code = -1;

void done_handler(int exitcode, int bysignal, void *user_data) {
    child_pid = -1;
    exited_with_code = exitcode;
    assert(user_data==(void*)0xdeadbeef);
    UNUSED(bysignal);
}

int fork_create(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    long long code_to_exit_with;
    long long usleep_us;
    if (argc != 3) {
        NexCacheModule_WrongArity(ctx);
        return NEXCACHEMODULE_OK;
    }

    if(!RMAPI_FUNC_SUPPORTED(NexCacheModule_Fork)){
        NexCacheModule_ReplyWithError(ctx, "Fork api is not supported in the current nexcache version");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModule_StringToLongLong(argv[1], &code_to_exit_with);
    NexCacheModule_StringToLongLong(argv[2], &usleep_us);
    exited_with_code = -1;
    int fork_child_pid = NexCacheModule_Fork(done_handler, (void*)0xdeadbeef);
    if (fork_child_pid < 0) {
        NexCacheModule_ReplyWithError(ctx, "Fork failed");
        return NEXCACHEMODULE_OK;
    } else if (fork_child_pid > 0) {
        /* parent */
        child_pid = fork_child_pid;
        NexCacheModule_ReplyWithLongLong(ctx, child_pid);
        return NEXCACHEMODULE_OK;
    }

    /* child */
    NexCacheModule_Log(ctx, "notice", "fork child started");
    usleep(usleep_us);
    NexCacheModule_Log(ctx, "notice", "fork child exiting");
    NexCacheModule_ExitFromChild(code_to_exit_with);
    /* unreachable */
    return 0;
}

int fork_exitcode(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    UNUSED(argv);
    UNUSED(argc);
    NexCacheModule_ReplyWithLongLong(ctx, exited_with_code);
    return NEXCACHEMODULE_OK;
}

int fork_kill(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc)
{
    UNUSED(argv);
    UNUSED(argc);
    if (NexCacheModule_KillForkChild(child_pid) != NEXCACHEMODULE_OK)
        NexCacheModule_ReplyWithError(ctx, "KillForkChild failed");
    else
        NexCacheModule_ReplyWithLongLong(ctx, 1);
    child_pid = -1;
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    UNUSED(argv);
    UNUSED(argc);
    if (NexCacheModule_Init(ctx,"fork",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fork.create", fork_create,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fork.exitcode", fork_exitcode,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"fork.kill", fork_kill,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
