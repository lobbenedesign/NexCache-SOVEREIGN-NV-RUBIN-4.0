
#include "nexcachemodule.h"
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

/* A wrap for SET command with ACL check on the key. */
int set_aclcheck_key(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 4) {
        return NexCacheModule_WrongArity(ctx);
    }

    int permissions;
    const char *flags = NexCacheModule_StringPtrLen(argv[1], NULL);

    if (!strcasecmp(flags, "W")) {
        permissions = NEXCACHEMODULE_CMD_KEY_UPDATE;
    } else if (!strcasecmp(flags, "R")) {
        permissions = NEXCACHEMODULE_CMD_KEY_ACCESS;
    } else if (!strcasecmp(flags, "*")) {
        permissions = NEXCACHEMODULE_CMD_KEY_UPDATE | NEXCACHEMODULE_CMD_KEY_ACCESS;
    } else if (!strcasecmp(flags, "~")) {
        permissions = 0; /* Requires either read or write */
    } else {
        NexCacheModule_ReplyWithError(ctx, "INVALID FLAGS");
        return NEXCACHEMODULE_OK;
    }

    /* Check that the key can be accessed */
    NexCacheModuleString *user_name = NexCacheModule_GetCurrentUserName(ctx);
    NexCacheModuleUser *user = NexCacheModule_GetModuleUserFromUserName(user_name);
    int ret = NexCacheModule_ACLCheckKeyPermissions(user, argv[2], permissions);
    if (ret != 0) {
        NexCacheModule_ReplyWithError(ctx, "DENIED KEY");
        NexCacheModule_FreeModuleUser(user);
        NexCacheModule_FreeString(ctx, user_name);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "SET", "v", argv + 2, (size_t)argc - 2);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    NexCacheModule_FreeModuleUser(user);
    NexCacheModule_FreeString(ctx, user_name);
    return NEXCACHEMODULE_OK;
}

/* A wrap for PUBLISH command with ACL check on the channel. */
int publish_aclcheck_channel(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc != 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    /* Check that the pubsub channel can be accessed */
    NexCacheModuleString *user_name = NexCacheModule_GetCurrentUserName(ctx);
    NexCacheModuleUser *user = NexCacheModule_GetModuleUserFromUserName(user_name);
    int ret = NexCacheModule_ACLCheckChannelPermissions(user, argv[1], NEXCACHEMODULE_CMD_CHANNEL_SUBSCRIBE);
    if (ret != 0) {
        NexCacheModule_ReplyWithError(ctx, "DENIED CHANNEL");
        NexCacheModule_FreeModuleUser(user);
        NexCacheModule_FreeString(ctx, user_name);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, "PUBLISH", "v", argv + 1, (size_t)argc - 1);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    NexCacheModule_FreeModuleUser(user);
    NexCacheModule_FreeString(ctx, user_name);
    return NEXCACHEMODULE_OK;
}


/* ACL check that validates command execution with all permissions
 * including command, keys, channels, and database access */
int aclcheck_check_permissions(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    if (argc < 3) {
        return NexCacheModule_WrongArity(ctx);
    }

    long long dbid;
    if (NexCacheModule_StringToLongLong(argv[1], &dbid) != NEXCACHEMODULE_OK) {
        NexCacheModule_ReplyWithError(ctx, "invalid DB index");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString *user_name = NexCacheModule_GetCurrentUserName(ctx);
    NexCacheModuleUser *user = NexCacheModule_GetModuleUserFromUserName(user_name);

    NexCacheModuleACLLogEntryReason denial_reason;
    int ret = NexCacheModule_ACLCheckPermissions(user, argv + 2, argc - 2, (int)dbid, &denial_reason);

    if (ret != NEXCACHEMODULE_OK) {
        int saved_errno = errno;
        if (saved_errno == EINVAL) {
            NexCacheModule_ReplyWithError(ctx, "ERR invalid arguments");
        } else if (saved_errno == EACCES) {
            NexCacheModule_ReplyWithError(ctx, "NOPERM");
            NexCacheModuleString *obj;
            switch (denial_reason) {
                case NEXCACHEMODULE_ACL_LOG_CMD:
                    obj = argv[2];
                    break;
                case NEXCACHEMODULE_ACL_LOG_KEY:
                    obj = (argc > 3) ? argv[3] : argv[2];
                    break;
                case NEXCACHEMODULE_ACL_LOG_CHANNEL:
                    obj = (argc > 3) ? argv[3] : argv[2];
                    break;
                case NEXCACHEMODULE_ACL_LOG_DB:
                    obj = argv[1];
                    break;
                default:
                    obj = argv[2];
                    break;
            }
            NexCacheModule_ACLAddLogEntry(ctx, user, obj, denial_reason);
        } else {
            NexCacheModule_ReplyWithError(ctx, "ERR unexpected error");
        }
        NexCacheModule_FreeModuleUser(user);
        NexCacheModule_FreeString(ctx, user_name);
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleCallReply *rep = NexCacheModule_Call(ctx, NexCacheModule_StringPtrLen(argv[2], NULL), "v", argv + 3, (size_t)argc - 3);
    if (!rep) {
        NexCacheModule_ReplyWithError(ctx, "NULL reply");
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    NexCacheModule_FreeModuleUser(user);
    NexCacheModule_FreeString(ctx, user_name);
    return NEXCACHEMODULE_OK;
}

/* A wrap for RM_Call that check first that the command can be executed */
int rm_call_aclcheck_cmd(NexCacheModuleCtx *ctx, NexCacheModuleUser *user, NexCacheModuleString **argv, int argc) {
    if (argc < 2) {
        return NexCacheModule_WrongArity(ctx);
    }

    /* Check that the command can be executed */
    int ret = NexCacheModule_ACLCheckCommandPermissions(user, argv + 1, argc - 1);
    if (ret != 0) {
        NexCacheModule_ReplyWithError(ctx, "DENIED CMD");
        /* Add entry to ACL log */
        NexCacheModule_ACLAddLogEntry(ctx, user, argv[1], NEXCACHEMODULE_ACL_LOG_CMD);
        return NEXCACHEMODULE_OK;
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "v", argv + 2, (size_t)argc - 2);
    if(!rep){
        NexCacheModule_ReplyWithError(ctx, "NULL reply returned");
    }else{
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

int rm_call_aclcheck_cmd_default_user(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NexCacheModuleString *user_name = NexCacheModule_GetCurrentUserName(ctx);
    NexCacheModuleUser *user = NexCacheModule_GetModuleUserFromUserName(user_name);

    int res = rm_call_aclcheck_cmd(ctx, user, argv, argc);

    NexCacheModule_FreeModuleUser(user);
    NexCacheModule_FreeString(ctx, user_name);
    return res;
}

int rm_call_aclcheck_cmd_module_user(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    /* Create a user and authenticate */
    NexCacheModuleUser *user = NexCacheModule_CreateModuleUser("testuser1");
    NexCacheModule_SetModuleUserACL(user, "allcommands");
    NexCacheModule_SetModuleUserACL(user, "allkeys");
    NexCacheModule_SetModuleUserACL(user, "on");
    NexCacheModule_AuthenticateClientWithUser(ctx, user, NULL, NULL, NULL);

    int res = rm_call_aclcheck_cmd(ctx, user, argv, argc);

    /* authenticated back to "default" user (so once we free testuser1 we will not disconnected */
    NexCacheModule_AuthenticateClientWithACLUser(ctx, "default", 7, NULL, NULL, NULL);
    NexCacheModule_FreeModuleUser(user);
    return res;
}

int rm_call_aclcheck_with_errors(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "vEC", argv + 2, (size_t)argc - 2);
    NexCacheModule_ReplyWithCallReply(ctx, rep);
    NexCacheModule_FreeCallReply(rep);
    return NEXCACHEMODULE_OK;
}

/* A wrap for RM_Call that pass the 'C' flag to do ACL check on the command. */
int rm_call_aclcheck(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc){
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if(argc < 2){
        return NexCacheModule_WrongArity(ctx);
    }

    const char* cmd = NexCacheModule_StringPtrLen(argv[1], NULL);

    NexCacheModuleCallReply* rep = NexCacheModule_Call(ctx, cmd, "vC", argv + 2, (size_t)argc - 2);
    if(!rep) {
        char err[100];
        switch (errno) {
            case EACCES:
                NexCacheModule_ReplyWithError(ctx, "ERR NOPERM");
                break;
            default:
                snprintf(err, sizeof(err) - 1, "ERR errno=%d", errno);
                NexCacheModule_ReplyWithError(ctx, err);
                break;
        }
    } else {
        NexCacheModule_ReplyWithCallReply(ctx, rep);
        NexCacheModule_FreeCallReply(rep);
    }

    return NEXCACHEMODULE_OK;
}

int module_check_key_permission(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if(argc != 3){
        return NexCacheModule_WrongArity(ctx);
    }

    size_t key_len = 0;
    unsigned int flags = 0;
    const char* key_prefix = NexCacheModule_StringPtrLen(argv[1], &key_len);
    const char* check_what = NexCacheModule_StringPtrLen(argv[2], NULL);
    if (strcasecmp(check_what, "R") == 0) {
        flags = NEXCACHEMODULE_CMD_KEY_ACCESS;
    } else if(strcasecmp(check_what, "W") == 0) {
        flags = NEXCACHEMODULE_CMD_KEY_INSERT | NEXCACHEMODULE_CMD_KEY_DELETE | NEXCACHEMODULE_CMD_KEY_UPDATE;
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "EINVALID");
        return NEXCACHEMODULE_OK;
    }

    NexCacheModuleString* user_name = NexCacheModule_GetCurrentUserName(ctx);
    NexCacheModuleUser *user = NexCacheModule_GetModuleUserFromUserName(user_name);
    int rc = NexCacheModule_ACLCheckKeyPrefixPermissions(user, key_prefix, key_len, flags);
    if (rc == NEXCACHEMODULE_OK) {
        // Access granted.
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        // Access denied.
        NexCacheModule_ReplyWithSimpleString(ctx, "EACCESS");
    }
    NexCacheModule_FreeModuleUser(user);
    NexCacheModule_FreeString(ctx, user_name);
    return NEXCACHEMODULE_OK;
}

int module_test_acl_category(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    return NEXCACHEMODULE_OK;
}

int commandBlockCheck(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);
    int response_ok = 0;
    int result = NexCacheModule_CreateCommand(ctx,"command.that.should.fail", module_test_acl_category, "", 0, 0, 0);
    response_ok |= (result == NEXCACHEMODULE_OK);

    result = NexCacheModule_AddACLCategory(ctx,"blockedcategory");
    response_ok |= (result == NEXCACHEMODULE_OK);
    
    NexCacheModuleCommand *parent = NexCacheModule_GetCommand(ctx,"block.commands.outside.onload");
    result = NexCacheModule_SetCommandACLCategories(parent, "write");
    response_ok |= (result == NEXCACHEMODULE_OK);

    result = NexCacheModule_CreateSubcommand(parent,"subcommand.that.should.fail",module_test_acl_category,"",0,0,0);
    response_ok |= (result == NEXCACHEMODULE_OK);
    
    /* This validates that it's not possible to create commands or add
     * a new ACL Category outside OnLoad function.
     * thus returns an error if they succeed. */
    if (response_ok) {
        NexCacheModule_ReplyWithError(ctx, "UNEXPECTEDOK");
    } else {
        NexCacheModule_ReplyWithSimpleString(ctx, "OK");
    }
    return NEXCACHEMODULE_OK;
}

int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {

    if (NexCacheModule_Init(ctx,"aclcheck",1,NEXCACHEMODULE_APIVER_1)== NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (argc > 1) return NexCacheModule_WrongArity(ctx);
    
    /* When that flag is passed, we try to create too many categories,
     * and the test expects this to fail. In this case the server returns NEXCACHEMODULE_ERR
     * and set errno to ENOMEM*/
    if (argc == 1) {
        long long fail_flag = 0;
        NexCacheModule_StringToLongLong(argv[0], &fail_flag);
        if (fail_flag) {
            for (size_t j = 0; j < 45; j++) {
                NexCacheModuleString* name =  NexCacheModule_CreateStringPrintf(ctx, "customcategory%zu", j);
                if (NexCacheModule_AddACLCategory(ctx, NexCacheModule_StringPtrLen(name, NULL)) == NEXCACHEMODULE_ERR) {
                    NexCacheModule_Assert(errno == ENOMEM);
                    NexCacheModule_FreeString(ctx, name);
                    return NEXCACHEMODULE_ERR;
                }
                NexCacheModule_FreeString(ctx, name);
            }
        }
    }

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.set.check.key", set_aclcheck_key,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"block.commands.outside.onload", commandBlockCheck,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.module.command.aclcategories.write", module_test_acl_category,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    NexCacheModuleCommand *aclcategories_write = NexCacheModule_GetCommand(ctx,"aclcheck.module.command.aclcategories.write");

    if (NexCacheModule_SetCommandACLCategories(aclcategories_write, "write") == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.module.command.aclcategories.write.function.read.category", module_test_acl_category,"write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    NexCacheModuleCommand *read_category = NexCacheModule_GetCommand(ctx,"aclcheck.module.command.aclcategories.write.function.read.category");

    if (NexCacheModule_SetCommandACLCategories(read_category, "read") == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.module.command.aclcategories.read.only.category", module_test_acl_category,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    NexCacheModuleCommand *read_only_category = NexCacheModule_GetCommand(ctx,"aclcheck.module.command.aclcategories.read.only.category");

    if (NexCacheModule_SetCommandACLCategories(read_only_category, "read") == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.publish.check.channel", publish_aclcheck_channel,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.check.permissions", aclcheck_check_permissions,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.rm_call.check.cmd", rm_call_aclcheck_cmd_default_user,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.rm_call.check.cmd.module.user", rm_call_aclcheck_cmd_module_user,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.rm_call", rm_call_aclcheck,
                                  "write",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"aclcheck.rm_call_with_errors", rm_call_aclcheck_with_errors,
                                      "write",0,0,0) == NEXCACHEMODULE_ERR)
            return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx,"acl.check_key_prefix", 
                                   module_check_key_permission, 
                                   "", 
                                   0, 
                                   0, 
                                   0) == NEXCACHEMODULE_ERR) {
        return NEXCACHEMODULE_ERR;
    } 
    /* This validates that, when module tries to add a category with invalid characters,
     * the server returns NEXCACHEMODULE_ERR and set errno to `EINVAL` */
    if (NexCacheModule_AddACLCategory(ctx,"!nval!dch@r@cter$") == NEXCACHEMODULE_ERR)
        NexCacheModule_Assert(errno == EINVAL);
    else 
        return NEXCACHEMODULE_ERR;
    
    /* This validates that, when module tries to add a category that already exists,
     * the server returns NEXCACHEMODULE_ERR and set errno to `EBUSY` */
    if (NexCacheModule_AddACLCategory(ctx,"write") == NEXCACHEMODULE_ERR)
        NexCacheModule_Assert(errno == EBUSY);
    else 
        return NEXCACHEMODULE_ERR;
    
    if (NexCacheModule_AddACLCategory(ctx,"foocategory") == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    
    if (NexCacheModule_CreateCommand(ctx,"aclcheck.module.command.test.add.new.aclcategories", module_test_acl_category,"",0,0,0) == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    NexCacheModuleCommand *test_add_new_aclcategories = NexCacheModule_GetCommand(ctx,"aclcheck.module.command.test.add.new.aclcategories");

    if (NexCacheModule_SetCommandACLCategories(test_add_new_aclcategories, "foocategory") == NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;
    
    return NEXCACHEMODULE_OK;
}
