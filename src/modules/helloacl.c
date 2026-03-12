/* ACL API example - An example for performing custom synchronous and
 * asynchronous password authentication.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright 2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * NexCachetribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * NexCachetributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * NexCachetributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NexCache nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../nexcachemodule.h"
#include <pthread.h>
#include <unistd.h>

// A simple global user
static NexCacheModuleUser *global;
static uint64_t global_auth_client_id = 0;

/* HELLOACL.REVOKE
 * Synchronously revoke access from a user. */
int RevokeCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (global_auth_client_id) {
        NexCacheModule_DeauthenticateAndCloseClient(ctx, global_auth_client_id);
        return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        return NexCacheModule_ReplyWithError(ctx, "Global user currently not used");
    }
}

/* HELLOACL.RESET
 * Synchronously delete and re-create a module user. */
int ResetCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_FreeModuleUser(global);
    global = NexCacheModule_CreateModuleUser("global");
    NexCacheModule_SetModuleUserACL(global, "allcommands");
    NexCacheModule_SetModuleUserACL(global, "allkeys");
    NexCacheModule_SetModuleUserACL(global, "on");

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* Callback handler for user changes, use this to notify a module of
 * changes to users authenticated by the module */
void HelloACL_UserChanged(uint64_t client_id, void *privdata) {
    NEXCACHEMODULE_NOT_USED(privdata);
    NEXCACHEMODULE_NOT_USED(client_id);
    global_auth_client_id = 0;
}

/* HELLOACL.AUTHGLOBAL
 * Synchronously assigns a module user to the current context. */
int AuthGlobalCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (global_auth_client_id) {
        return NexCacheModule_ReplyWithError(ctx, "Global user currently used");
    }

    NexCacheModule_AuthenticateClientWithUser(ctx, global, HelloACL_UserChanged, NULL, &global_auth_client_id);

    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

#define TIMEOUT_TIME 1000

/* Reply callback for auth command HELLOACL.AUTHASYNC */
int HelloACL_Reply(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    size_t length;

    NexCacheModuleString *user_string = NexCacheModule_GetBlockedClientPrivateData(ctx);
    const char *name = NexCacheModule_StringPtrLen(user_string, &length);

    if (NexCacheModule_AuthenticateClientWithACLUser(ctx, name, length, NULL, NULL, NULL) == NEXCACHEMODULE_ERR) {
        return NexCacheModule_ReplyWithError(ctx, "Invalid Username or password");
    }
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* Timeout callback for auth command HELLOACL.AUTHASYNC */
int HelloACL_Timeout(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    return NexCacheModule_ReplyWithSimpleString(ctx, "Request timedout");
}

/* Private data frees data for HELLOACL.AUTHASYNC command. */
void HelloACL_FreeData(NexCacheModuleCtx *ctx, void *privdata) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NexCacheModule_FreeString(NULL, privdata);
}

/* Background authentication can happen here. */
void *HelloACL_ThreadMain(void *args) {
    void **targs = args;
    NexCacheModuleBlockedClient *bc = targs[0];
    NexCacheModuleString *user = targs[1];
    NexCacheModule_Free(targs);

    NexCacheModule_UnblockClient(bc, user);
    return NULL;
}

/* HELLOACL.AUTHASYNC
 * Asynchronously assigns an ACL user to the current context. */
int AuthAsyncCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 2) return NexCacheModule_WrongArity(ctx);

    pthread_t tid;
    NexCacheModuleBlockedClient *bc =
        NexCacheModule_BlockClient(ctx, HelloACL_Reply, HelloACL_Timeout, HelloACL_FreeData, TIMEOUT_TIME);


    void **targs = NexCacheModule_Alloc(sizeof(void *) * 2);
    targs[0] = bc;
    targs[1] = NexCacheModule_CreateStringFromString(NULL, argv[1]);

    if (pthread_create(&tid, NULL, HelloACL_ThreadMain, targs) != 0) {
        NexCacheModule_AbortBlock(bc);
        return NexCacheModule_ReplyWithError(ctx, "-ERR Can't start thread");
    }

    return NEXCACHEMODULE_OK;
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "helloacl", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "helloacl.reset", ResetCommand_NexCacheCommand, "", 0, 0, 0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "helloacl.revoke", RevokeCommand_NexCacheCommand, "", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "helloacl.authglobal", AuthGlobalCommand_NexCacheCommand, "no-auth", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "helloacl.authasync", AuthAsyncCommand_NexCacheCommand, "no-auth", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    global = NexCacheModule_CreateModuleUser("global");
    NexCacheModule_SetModuleUserACL(global, "allcommands");
    NexCacheModule_SetModuleUserACL(global, "allkeys");
    NexCacheModule_SetModuleUserACL(global, "on");

    global_auth_client_id = 0;

    return NEXCACHEMODULE_OK;
}
