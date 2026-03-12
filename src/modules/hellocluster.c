/* Helloworld cluster -- A ping/pong cluster API example.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2018, NexCache Contributors.
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MSGTYPE_PING 1
#define MSGTYPE_PONG 2

/* HELLOCLUSTER.PINGALL */
int PingallCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    NexCacheModule_SendClusterMessage(ctx, NULL, MSGTYPE_PING, "Hey", 3);
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* HELLOCLUSTER.LIST */
int ListCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    size_t numnodes;
    char **ids = NexCacheModule_GetClusterNodesList(ctx, &numnodes);
    if (ids == NULL) {
        return NexCacheModule_ReplyWithError(ctx, "Cluster not enabled");
    }

    NexCacheModule_ReplyWithArray(ctx, numnodes);
    for (size_t j = 0; j < numnodes; j++) {
        int port;
        NexCacheModule_GetClusterNodeInfo(ctx, ids[j], NULL, NULL, &port, NULL);
        NexCacheModule_ReplyWithArray(ctx, 2);
        NexCacheModule_ReplyWithStringBuffer(ctx, ids[j], NEXCACHEMODULE_NODE_ID_LEN);
        NexCacheModule_ReplyWithLongLong(ctx, port);
    }
    NexCacheModule_FreeClusterNodesList(ids);
    return NEXCACHEMODULE_OK;
}

/* Callback for message MSGTYPE_PING */
void PingReceiver(NexCacheModuleCtx *ctx,
                  const char *sender_id,
                  uint8_t type,
                  const unsigned char *payload,
                  uint32_t len) {
    NexCacheModule_Log(ctx, "notice", "PING (type %d) RECEIVED from %.*s: '%.*s'", type, NEXCACHEMODULE_NODE_ID_LEN,
                     sender_id, (int)len, payload);
    NexCacheModule_SendClusterMessage(ctx, NULL, MSGTYPE_PONG, "Ohi!", 4);
    NexCacheModuleCallReply *reply = NexCacheModule_Call(ctx, "INCR", "c", "pings_received");
    NexCacheModule_FreeCallReply(reply);
}

/* Callback for message MSGTYPE_PONG. */
void PongReceiver(NexCacheModuleCtx *ctx,
                  const char *sender_id,
                  uint8_t type,
                  const unsigned char *payload,
                  uint32_t len) {
    NexCacheModule_Log(ctx, "notice", "PONG (type %d) RECEIVED from %.*s: '%.*s'", type, NEXCACHEMODULE_NODE_ID_LEN,
                     sender_id, (int)len, payload);
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "hellocluster", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellocluster.pingall", PingallCommand_NexCacheCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellocluster.list", ListCommand_NexCacheCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    /* Disable Cluster sharding and redirections. This way every node
     * will be able to access every possible key, regardless of the hash slot.
     * This way the PING message handler will be able to increment a specific
     * variable. Normally you do that in order for the distributed system
     * you create as a module to have total freedom in the keyspace
     * manipulation. */
    NexCacheModule_SetClusterFlags(ctx, NEXCACHEMODULE_CLUSTER_FLAG_NO_REDIRECTION);

    /* Register our handlers for different message types. */
    NexCacheModule_RegisterClusterMessageReceiver(ctx, MSGTYPE_PING, PingReceiver);
    NexCacheModule_RegisterClusterMessageReceiver(ctx, MSGTYPE_PONG, PongReceiver);
    return NEXCACHEMODULE_OK;
}
