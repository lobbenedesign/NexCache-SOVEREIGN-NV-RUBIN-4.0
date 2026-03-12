#include <nexcache/async.h>
#include <nexcache/tls.h>
#include <nexcache/nexcache.h>

#include <nexcache/adapters/libevent.h>

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

    struct event_base *base = event_base_new();
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <key> <host> <port> <cert> <certKey> [ca]\n", argv[0]);
        exit(1);
    }

    const char *value = argv[1];
    size_t nvalue = strlen(value);

    const char *hostname = argv[2];
    int port = atoi(argv[3]);

    const char *cert = argv[4];
    const char *certKey = argv[5];
    const char *caCert = argc > 5 ? argv[6] : NULL;

    nexcacheTLSContext *tls;
    nexcacheTLSContextError tls_error = NEXCACHE_TLS_CTX_NONE;

    nexcacheInitOpenSSL();

    tls = nexcacheCreateTLSContext(caCert, NULL,
                                 cert, certKey, NULL, &tls_error);
    if (!tls) {
        printf("Error: %s\n", nexcacheTLSContextGetError(tls_error));
        return 1;
    }

    nexcacheAsyncContext *c = nexcacheAsyncConnect(hostname, port);
    if (c->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c->errstr);
        return 1;
    }
    if (nexcacheInitiateTLSWithContext(&c->c, tls) != NEXCACHE_OK) {
        printf("TLS Error!\n");
        exit(1);
    }

    nexcacheLibeventAttach(c, base);
    nexcacheAsyncSetConnectCallback(c, connectCallback);
    nexcacheAsyncSetDisconnectCallback(c, disconnectCallback);
    nexcacheAsyncCommand(c, NULL, NULL, "SET key %b", value, nvalue);
    nexcacheAsyncCommand(c, getCallback, (char *)"end-1", "GET key");
    event_base_dispatch(base);

    nexcacheFreeTLSContext(tls);
    return 0;
}
