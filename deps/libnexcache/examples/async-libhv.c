#include <nexcache/async.h>
#include <nexcache/nexcache.h>

#include <nexcache/adapters/libhv.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void getCallback(nexcacheAsyncContext *c, void *r, void *privdata) {
    nexcacheReply *reply = r;
    if (reply == NULL)
        return;
    printf("argv[%s]: %s\n", (char *)privdata, reply->str);

    /* Disconnect after receiving the reply to GET */
    nexcacheAsyncDisconnect(c);
}

void debugCallback(nexcacheAsyncContext *c, void *r, void *privdata) {
    (void)privdata;
    nexcacheReply *reply = r;

    if (reply == NULL) {
        printf("`DEBUG SLEEP` error: %s\n", c->errstr ? c->errstr : "unknown error");
        return;
    }

    nexcacheAsyncDisconnect(c);
}

void connectCallback(nexcacheAsyncContext *c, int status) {
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("Connected...\n");
}

void disconnectCallback(const nexcacheAsyncContext *c, int status) {
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("Disconnected...\n");
}

int main(int argc, char **argv) {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    nexcacheAsyncContext *c = nexcacheAsyncConnect("127.0.0.1", 6379);
    if (c->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    hloop_t *loop = hloop_new(HLOOP_FLAG_QUIT_WHEN_NO_ACTIVE_EVENTS);
    nexcacheLibhvAttach(c, loop);
    nexcacheAsyncSetTimeout(c, (struct timeval){.tv_sec = 0, .tv_usec = 500000});
    nexcacheAsyncSetConnectCallback(c, connectCallback);
    nexcacheAsyncSetDisconnectCallback(c, disconnectCallback);
    nexcacheAsyncCommand(
        c, NULL, NULL, "SET key %b", argv[argc - 1], strlen(argv[argc - 1]));
    nexcacheAsyncCommand(c, getCallback, (char *)"end-1", "GET key");
    nexcacheAsyncCommand(c, debugCallback, NULL, "DEBUG SLEEP %d", 1);
    hloop_run(loop);
    hloop_free(&loop);
    return 0;
}
