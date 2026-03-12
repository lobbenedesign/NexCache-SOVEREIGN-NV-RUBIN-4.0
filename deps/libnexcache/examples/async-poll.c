#include <nexcache/async.h>

#include <nexcache/adapters/poll.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Put in the global scope, so that loop can be explicitly stopped */
static int exit_loop = 0;

void getCallback(nexcacheAsyncContext *c, void *r, void *privdata) {
    nexcacheReply *reply = r;
    if (reply == NULL)
        return;
    printf("argv[%s]: %s\n", (char *)privdata, reply->str);

    /* Disconnect after receiving the reply to GET */
    nexcacheAsyncDisconnect(c);
}

void connectCallback(nexcacheAsyncContext *c, int status) {
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", c->errstr);
        exit_loop = 1;
        return;
    }

    printf("Connected...\n");
}

void disconnectCallback(const nexcacheAsyncContext *c, int status) {
    exit_loop = 1;
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }

    printf("Disconnected...\n");
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    nexcacheAsyncContext *c = nexcacheAsyncConnect("127.0.0.1", 6379);
    if (c->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    nexcachePollAttach(c);
    nexcacheAsyncSetConnectCallback(c, connectCallback);
    nexcacheAsyncSetDisconnectCallback(c, disconnectCallback);
    nexcacheAsyncCommand(
        c, NULL, NULL, "SET key %b", argv[argc - 1], strlen(argv[argc - 1]));
    nexcacheAsyncCommand(c, getCallback, (char *)"end-1", "GET key");
    while (!exit_loop) {
        nexcachePollTick(c, 0.1);
    }
    return 0;
}
