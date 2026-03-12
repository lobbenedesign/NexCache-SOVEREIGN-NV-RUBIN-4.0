#include "nexcachemodule.h"

#include <string.h>

/* This is a second sample module to validate that module authentication callbacks can be registered
 * from multiple modules. */

/* Non Blocking Module Auth callback / implementation. */
int auth_cb(NexCacheModuleCtx *ctx, NexCacheModuleString *username, NexCacheModuleString *password, NexCacheModuleString **err) {
    const char *user = NexCacheModule_StringPtrLen(username, NULL);
    const char *pwd = NexCacheModule_StringPtrLen(password, NULL);
    if (!strcmp(user,"foo") && !strcmp(pwd,"allow_two")) {
        NexCacheModule_AuthenticateClientWithACLUser(ctx, "foo", 3, NULL, NULL, NULL);
        return NEXCACHEMODULE_AUTH_HANDLED;
    }
    else if (!strcmp(user,"foo") && !strcmp(pwd,"deny_two")) {
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

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    if (NexCacheModule_Init(ctx,"moduleauthtwo",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    if (NexCacheModule_CreateCommand(ctx,"testmoduletwo.rm_register_auth_cb", test_rm_register_auth_cb,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    return NEXCACHEMODULE_OK;
}