#include <nexcache/cluster.h>
#include <nexcache/tls.h>

#include <nexcache/adapters/libevent.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_NODE_TLS "127.0.0.1:7300"

void getCallback(nexcacheClusterAsyncContext *cc, void *r, void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    if (reply == NULL) {
        if (cc->err) {
            printf("errstr: %s\n", cc->errstr);
        }
        return;
    }
    printf("privdata: %s reply: %s\n", (char *)privdata, reply->str);

    /* Disconnect after receiving the reply to GET */
    nexcacheClusterAsyncDisconnect(cc);
}

void setCallback(nexcacheClusterAsyncContext *cc, void *r, void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    if (reply == NULL) {
        if (cc->err) {
            printf("errstr: %s\n", cc->errstr);
        }
        return;
    }
    printf("privdata: %s reply: %s\n", (char *)privdata, reply->str);
}

void connectCallback(nexcacheAsyncContext *ac, int status) {
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", ac->errstr);
        return;
    }

    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const nexcacheAsyncContext *ac, int status) {
    if (status != NEXCACHE_OK) {
        printf("Error: %s\n", ac->errstr);
        return;
    }
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

int main(void) {
    nexcacheTLSContext *tls;
    nexcacheTLSContextError tls_error;

    nexcacheInitOpenSSL();
    tls = nexcacheCreateTLSContext("ca.crt", NULL, "client.crt", "client.key",
                                 NULL, &tls_error);
    if (!tls) {
        printf("TLS Context error: %s\n", nexcacheTLSContextGetError(tls_error));
        exit(1);
    }

    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_TLS;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    options.tls = tls;
    options.tls_init_fn = &nexcacheInitiateTLSWithContext;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    if (acc == NULL || acc->err != 0) {
        printf("Error: %s\n", acc ? acc->errstr : "OOM");
        exit(-1);
    }

    int status;
    status = nexcacheClusterAsyncCommand(acc, setCallback, (char *)"THE_ID",
                                       "SET %s %s", "key", "value");
    if (status != NEXCACHE_OK) {
        printf("error: err=%d errstr=%s\n", acc->err, acc->errstr);
    }

    status = nexcacheClusterAsyncCommand(acc, getCallback, (char *)"THE_ID",
                                       "GET %s", "key");
    if (status != NEXCACHE_OK) {
        printf("error: err=%d errstr=%s\n", acc->err, acc->errstr);
    }

    printf("Dispatch..\n");
    event_base_dispatch(base);

    printf("Done..\n");
    nexcacheClusterAsyncFree(acc);
    nexcacheFreeTLSContext(tls);
    event_base_free(base);
    return 0;
}
