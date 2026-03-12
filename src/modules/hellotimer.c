/* Timer API example -- Register and handle timer events
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2018, Redis Ltd.
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

/* Timer callback. */
void timerHandler(NexCacheModuleCtx *ctx, void *data) {
    NEXCACHEMODULE_NOT_USED(ctx);
    printf("Fired %s!\n", (char *)data);
    NexCacheModule_Free(data);
}

/* HELLOTIMER.TIMER*/
int TimerCommand_NexCacheCommand(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    for (int j = 0; j < 10; j++) {
        int delay = rand() % 5000;
        char *buf = NexCacheModule_Alloc(256);
        snprintf(buf, 256, "After %d", delay);
        NexCacheModuleTimerID tid = NexCacheModule_CreateTimer(ctx, delay, timerHandler, buf);
        NEXCACHEMODULE_NOT_USED(tid);
    }
    return NexCacheModule_ReplyWithSimpleString(ctx, "OK");
}

/* This function must be present on each module. It is used in order to
 * register the commands into the server. */
int NexCacheModule_OnLoad(NexCacheModuleCtx *ctx, NexCacheModuleString **argv, int argc) {
    NEXCACHEMODULE_NOT_USED(argv);
    NEXCACHEMODULE_NOT_USED(argc);

    if (NexCacheModule_Init(ctx, "hellotimer", 1, NEXCACHEMODULE_APIVER_1) == NEXCACHEMODULE_ERR) return NEXCACHEMODULE_ERR;

    if (NexCacheModule_CreateCommand(ctx, "hellotimer.timer", TimerCommand_NexCacheCommand, "readonly", 0, 0, 0) ==
        NEXCACHEMODULE_ERR)
        return NEXCACHEMODULE_ERR;

    return NEXCACHEMODULE_OK;
}
