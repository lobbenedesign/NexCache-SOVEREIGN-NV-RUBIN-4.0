/* Server hooks API example
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2019, Redis Ltd.
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

/* Client state change callback. */
void clientChangeCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleClientInfo *ci = data;
    printf("Client %s event for client #%llu %s:%d\n",
           (sub == NEXCACHEMODULE_SUBEVENT_CLIENT_CHANGE_CONNECTED) ? "connection" : "disconnection",
           (unsigned long long)ci->id, ci->addr, ci->port);
}

void flushdbCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleFlushInfo *fi = data;
    if (sub == NEXCACHEMODULE_SUBEVENT_FLUSHDB_START) {
        if (fi->dbnum != -1) {
            NexCacheModuleCallReply *reply;
            reply = NexCacheModule_Call(ctx, "DBSIZE", "");
            long long numkeys = NexCacheModule_CallReplyInteger(reply);
            printf("FLUSHDB event of database %d started (%lld keys in DB)\n", fi->dbnum, numkeys);
            NexCacheModule_FreeCallReply(reply);
        } else {
            printf("FLUSHALL event started\n");
        }
    } else {
        if (fi->dbnum != -1) {
            printf("FLUSHDB event of database %d ended\n", fi->dbnum);
        } else {
            printf("FLUSHALL event ended\n");
        }
    }
}

void authenticationAttemptCallback(NexCacheModuleCtx *ctx, NexCacheModuleEvent e, uint64_t sub, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    NEXCACHEMODULE_NOT_USED(e);

    NexCacheModuleAuthenticationInfo *ai = data;
    printf("Authentication attempt for client #%llu with username=%s module=%s success=%d\n",
           (unsigned long long)ai->client_id, ai->username, ai->module_name, ai->result == NEXCACHEMODULE_AUTH_RESULT_GRANTED);
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "hellohook", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_ClientChange, clientChangeCallback);
    NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_FlushDB, flushdbCallback);
    NexCacheModule_SubscribeToServerEvent(ctx, NexCacheModuleEvent_AuthenticationAttempt, authenticationAttemptCallback);
    return NEXCACHEMODULE_OK;
}
