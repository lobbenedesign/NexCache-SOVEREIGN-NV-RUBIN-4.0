/* define macros for having usleep */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include "nexcachemodule.h"

#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define UNUSED(V) ((void) V)

// A simple global user
static NexCacheModuleUser *global = NULL;
static long long client_change_delta = 0;

void UserChangedCallback(uint64_t client_id, void *privdata) {
    NEXCACHEMODULE_NOT_USED(privdata);
    NEXCACHEMODULE_NOT_USED(client_id);
    client_change_delta++;
}

int Auth_CreateModuleUser(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (global) {
        NexCacheModule_FreeModuleUser(global);
    }

    global = NexCacheModule_CreateModuleUser("global");
    NexCacheModule_SetModuleUserACL(global, "allcommands");
    NexCacheModule_SetModuleUserACL(global, "allkeys");
    NexCacheModule_SetModuleUserACL(global, "on");

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

int Auth_AuthModuleUser(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    uint64_t client_id;
    NexCacheModule_AuthenticateClientWithUser(ctx, global, UserChangedCallback, NULL, &client_id);

    return NexCacheModule_ReplyWithLongLong(ctx, (uint64_t) client_id);
}

int Auth_AuthRealUser(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    size_t length;
    uint64_t client_id;

    NexCacheModuleString *user_string = argv[1];
    const char *name = NexCacheModule_StringPtrLen(user_string, &length);

    if (NexCacheModule_AuthenticateClientWithACLUser(ctx, name, length, 
            UserChangedCallback, NULL, &client_id) == NEXCACHEMODULE_ERR) {
        return NexCacheModule_ReplyWithError(ctx, "Invalid user");   
    }

    return NexCacheModule_ReplyWithLongLong(ctx, (uint64_t) client_id);
}

/* This command redacts every other arguments and returns OK */
int Auth_RedactedAPI(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    for(int i = argc - 1; i > 0; i -= 2) {
        int result = NexCacheModule_RedactClientCommandArgument(ctx, i);
        NexCacheModule_Assert(result == NEXCACHEMODULE_OK);
    }
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK"); 
}

int Auth_ChangeCount(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    long long result = client_change_delta;
    client_change_delta = 0;
    return NexCacheModule_ReplyWithLongLong(ctx, result);
}

/* The Module functionality below validates that module authentication callbacks can be registered
 * to support both non-blocking and blocking module based authentication. */

/* Non Blocking Module Auth callback / implementation. */
int auth_cb(NexCacheModuleCtx *ctx, NexCacheModuleString *username, NexCacheModuleString *password, NexCacheModuleString **err) {
    const char *user = NexCacheModule_StringPtrLen(username, NULL);
    const char *pwd = NexCacheModule_StringPtrLen(password, NULL);
    if (!strcmp(user,"foo") && !strcmp(pwd,"allow")) {
        NexCacheModule_AuthenticateClientWithACLUser(ctx, "foo", 3, NULL, NULL, NULL);
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    else if (!strcmp(user,"foo") && !strcmp(pwd,"deny")) {
        NexCacheModuleString *log = NexCacheModule_CreateString(ctx, "Module Auth", 11);
        NexCacheModule_ACLAddLogEntryByUserName(ctx, username, log, NEXCACHEMODULE_ACL_LOG_AUTH);
        NexCacheModule_FreeString(ctx, log);
        const char *err_msg = "Auth denied by Misc Module.";
        *err = NexCacheModule_CreateString(ctx, err_msg, strlen(err_msg));
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    return NEXCACHEMODULE_AUTH_NOT_HANDLED;
}

int test_rm_register_auth_cb(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_RegisterAuthCallback(ctx, auth_cb);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/*
 * The thread entry point that actually executes the blocking part of the AUTH command.
 * This function sleeps for 0.5 seconds and then unblocks the client which will later call
 * `AuthBlock_Reply`.
 * `arg` is expected to contain the NexCacheModuleBlockedClient, username, and password.
 */
void *AuthBlock_ThreadMain(void *arg) {
    usleep(500000);
    void **targ = arg;
    NexCacheModuleBlockedClient *bc = targ[0];
    int result = 2;
    const char *user = NexCacheModule_StringPtrLen(targ[1], NULL);
    const char *pwd = NexCacheModule_StringPtrLen(targ[2], NULL);
    if (!strcmp(user,"foo") && !strcmp(pwd,"block_allow")) {
        result = 1;
    }
    else if (!strcmp(user,"foo") && !strcmp(pwd,"block_deny")) {
        result = 0;
    }
    else if (!strcmp(user,"foo") && !strcmp(pwd,"block_abort")) {
        NexCacheModule_BlockedClientMeasureTimeEnd(bc);
        NexCacheModule_AbortBlock(bc);
        goto cleanup;
    }
    /* Provide the result to the blocking reply cb. */
    void **replyarg = NexCacheModule_Alloc(sizeof(void*));
    replyarg[0] = (void *) (uintptr_t) result;
    NexCacheModule_BlockedClientMeasureTimeEnd(bc);
    NexCacheModule_UnblockClient(bc, replyarg);
cleanup:
    /* Free the username and password and thread / arg data. */
    NexCacheModule_FreeString(NULL, targ[1]);
    NexCacheModule_FreeString(NULL, targ[2]);
    NexCacheModule_Free(targ);
    return NULL;
}

/*
 * Reply callback for a blocking AUTH command. This is called when the client is unblocked.
 */
int AuthBlock_Reply(NexCacheModuleCtx *ctx, NexCacheModuleString *username, NexCacheModuleString *password, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(password);
    void **targ = NexCacheModule_GetBlockedClientPrivateData(ctx);
    int result = (uintptr_t) targ[0];
    size_t userlen = 0;
    const char *user = NexCacheModule_StringPtrLen(username, &userlen);
    /* Handle the success case by authenticating. */
    if (result == 1) {
        NexCacheModule_AuthenticateClientWithACLUser(ctx, user, userlen, NULL, NULL, NULL);
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    /* Handle the Error case by denying auth */
    else if (result == 0) {
        NexCacheModuleString *log = NexCacheModule_CreateString(ctx, "Module Auth", 11);
        NexCacheModule_ACLAddLogEntryByUserName(ctx, username, log, NEXCACHEMODULE_ACL_LOG_AUTH);
        NexCacheModule_FreeString(ctx, log);
        const char *err_msg = "Auth denied by Misc Module.";
        *err = NexCacheModule_CreateString(ctx, err_msg, strlen(err_msg));
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    /* "Skip" Authentication */
    return NEXCACHEMODULE_AUTH_NOT_HANDLED;
}

/* Private data freeing callback for Module Auth. */
void AuthBlock_FreeData(NexCacheModuleCtx *ctx, void *privdata) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModule_Free(privdata);
}

/* Callback triggered when the engine attempts module auth
 * Return code here is one of the following: Auth succeeded, Auth denied,
 * Auth not handled, Auth blocked.
 * The Module can have auth succeed / denied here itself, but this is an example
 * of blocking module auth.
 */
int blocking_auth_cb(NexCacheModuleCtx *ctx, NexCacheModuleString *username, NexCacheModuleString *password, NexCacheModuleString **err) {
    NEXCACHEMODULE_NOT_USED(err);
    /* Block the client from the Module. */
    NexCacheModuleBlockedClient *bc = NexCacheModule_BlockClientOnAuth(ctx, AuthBlock_Reply, AuthBlock_FreeData);
    int ctx_flags = NexCacheModule_GetContextFlags(ctx);
    if (ctx_flags & NEXCACHEMODULE_CTX_FLAGS_MULTI || ctx_flags & NEXCACHEMODULE_CTX_FLAGS_LUA) {
        /* Clean up by using NexCacheModule_UnblockClient since we attempted blocking the client. */
        NexCacheModule_UnblockClient(bc, NULL);
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    NexCacheModule_BlockedClientMeasureTimeStart(bc);
    pthread_t tid;
    /* Allocate memory for information needed. */
    void **targ = NexCacheModule_Alloc(sizeof(void*)*3);
    targ[0] = bc;
    targ[1] = NexCacheModule_CreateStringFromString(NULL, username);
    targ[2] = NexCacheModule_CreateStringFromString(NULL, password);
    /* Create bg thread and pass the blockedclient, username and password to it. */
    if (pthread_create(&tid, NULL, AuthBlock_ThreadMain, targ) != 0) {
        NexCacheModule_AbortBlock(bc);
    }
    return NEXCACHEMODULE_AUTH_HANDLED;
}

int test_rm_register_blocking_auth_cb(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_RegisterAuthCallback(ctx, blocking_auth_cb);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx,"testacl",1,NEXCACHEMODULE_APIVER_1)
        == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"auth.authrealuser",
        Auth_AuthRealUser,"no-auth",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"auth.createmoduleuser",
        Auth_CreateModuleUser,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"auth.authmoduleuser",
        Auth_AuthModuleUser,"no-auth",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"auth.changecount",
        Auth_ChangeCount,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"auth.redact",
        Auth_RedactedAPI,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testmoduleone.rm_register_auth_cb",
        test_rm_register_auth_cb,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"testmoduleone.rm_register_blocking_auth_cb",
        test_rm_register_blocking_auth_cb,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnUnload(NexCacheModuleCtx *ctx) {
    UNUSED(ctx);

    if (global)
        NexCacheModule_FreeModuleUser(global);

    return NEXCACHEMODULE_OK;
}
