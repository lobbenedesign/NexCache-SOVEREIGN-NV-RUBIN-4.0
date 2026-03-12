#include <nexcache/async.h>
#include <nexcache/nexcache.h>

#include <nexcache/adapters/ivykis.h>

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

    iv_init();

    nexcacheAsyncContext *c = nexcacheAsyncConnect("127.0.0.1", 6379);
    if (c->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    nexcacheIvykisAttach(c);
    nexcacheAsyncSetConnectCallback(c, connectCallback);
    nexcacheAsyncSetDisconnectCallback(c, disconnectCallback);
    nexcacheAsyncCommand(
        c, NULL, NULL, "SET key %b", argv[argc - 1], strlen(argv[argc - 1]));
    nexcacheAsyncCommand(c, getCallback, (char *)"end-1", "GET key");

    iv_main();

    iv_deinit();

    return 0;
}
