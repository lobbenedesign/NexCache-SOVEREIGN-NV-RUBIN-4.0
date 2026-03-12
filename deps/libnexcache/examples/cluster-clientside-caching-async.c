/*
 * Simple example how to enable client tracking to implement client side caching.
 * Tracking can be enabled via a registered connect callback and invalidation
 * messages are received via the registered push callback.
 * The disconnect callback should also be used as an indication of invalidation.
 */
#include <nexcache/cluster.h>

#include <nexcache/adapters/libevent.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"
#define KEY "key:1"

void pushCallback(nexcacheAsyncContext *ac, void *r);
void setCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata);
void getCallback1(nexcacheClusterAsyncContext *acc, void *r, void *privdata);
void getCallback2(nexcacheClusterAsyncContext *acc, void *r, void *privdata);
void modifyKey(const char *key, const char *value);

/* The connect callback enables RESP3 and client tracking,
 * and sets the push callback in the libnexcache context. */
void connectCallback(nexcacheAsyncContext *ac, int status) {
    assert(status == NEXCACHE_OK);
    (void)status; /* Suppress unused warning when NDEBUG is defined. */
    nexcacheAsyncSetPushCallback(ac, pushCallback);
    nexcacheAsyncCommand(ac, NULL, NULL, "HELLO 3");
    nexcacheAsyncCommand(ac, NULL, NULL, "CLIENT TRACKING ON");
    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

/* The event callback issues a 'SET' command when the client is ready to accept
   commands. A reply is expected via a call to 'setCallback()' */
void eventCallback(const nexcacheClusterContext *cc, int event, void *privdata) {
    (void)cc;
    (void)privdata;
    /* Get the async context by a simple cast since in the Async API a
     * nexcacheClusterAsyncContext is an extension of the nexcacheClusterContext. */
    nexcacheClusterAsyncContext *acc = (nexcacheClusterAsyncContext *)cc;

    /* We send our commands when the client is ready to accept commands. */
    if (event == VALKEYCLUSTER_EVENT_READY) {
        printf("Client is ready to accept commands\n");

        int status =
            nexcacheClusterAsyncCommand(acc, setCallback, NULL, "SET %s 1", KEY);
        assert(status == NEXCACHE_OK);
        (void)status; /* Suppress unused warning when NDEBUG is defined. */
    }
}

/* Message callback for 'SET' commands. Issues a 'GET' command and a reply is
   expected as a call to 'getCallback1()' */
void setCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    (void)privdata;
    nexcacheReply *reply = (nexcacheReply *)r;
    assert(reply != NULL);
    printf("Callback for 'SET', reply: %s\n", reply->str);

    int status =
        nexcacheClusterAsyncCommand(acc, getCallback1, NULL, "GET %s", KEY);
    assert(status == NEXCACHE_OK);
    (void)status; /* Suppress unused warning when NDEBUG is defined. */
}

/* Message callback for the first 'GET' command. Modifies the key to
   trigger NexCache to send a key invalidation message and then sends another
   'GET' command. The invalidation message is received via the registered
   push callback. */
void getCallback1(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    (void)privdata;
    nexcacheReply *reply = (nexcacheReply *)r;
    assert(reply != NULL);

    printf("Callback for first 'GET', reply: %s\n", reply->str);

    /* Modify the key from another client which will invalidate a cached value.
       NexCache will send an invalidation message via a push message. */
    modifyKey(KEY, "99");

    int status =
        nexcacheClusterAsyncCommand(acc, getCallback2, NULL, "GET %s", KEY);
    assert(status == NEXCACHE_OK);
    (void)status; /* Suppress unused warning when NDEBUG is defined. */
}

/* Push message callback handling invalidation messages. */
void pushCallback(nexcacheAsyncContext *ac, void *r) {
    (void)ac;
    nexcacheReply *reply = r;
    if (!(reply->type == NEXCACHE_REPLY_PUSH && reply->elements == 2 &&
          reply->element[0]->type == NEXCACHE_REPLY_STRING &&
          !strncmp(reply->element[0]->str, "invalidate", 10) &&
          reply->element[1]->type == NEXCACHE_REPLY_ARRAY)) {
        /* Not an 'invalidate' message. Ignore. */
        return;
    }
    nexcacheReply *payload = reply->element[1];
    size_t i;
    for (i = 0; i < payload->elements; i++) {
        nexcacheReply *key = payload->element[i];
        if (key->type == NEXCACHE_REPLY_STRING)
            printf("Invalidate key '%.*s'\n", (int)key->len, key->str);
        else if (key->type == NEXCACHE_REPLY_NIL)
            printf("Invalidate all\n");
    }
}

/* Message callback for 'GET' commands. Exits program. */
void getCallback2(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    (void)privdata;
    nexcacheReply *reply = (nexcacheReply *)r;
    assert(reply != NULL);

    printf("Callback for second 'GET', reply: %s\n", reply->str);

    /* Exit the eventloop after a couple of sent commands. */
    nexcacheClusterAsyncDisconnect(acc);
}

/* A disconnect callback should invalidate all cached keys. */
void disconnectCallback(const nexcacheAsyncContext *ac, int status) {
    assert(status == NEXCACHE_OK);
    (void)status; /* Suppress unused warning when NDEBUG is defined. */
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);

    printf("Invalidate all\n");
}

/* Helper to modify keys using a separate client. */
void modifyKey(const char *key, const char *value) {
    printf("Modify key: '%s'\n", key);
    nexcacheClusterContext *cc = nexcacheClusterConnect(CLUSTER_NODE);
    assert(cc);

    nexcacheReply *reply = nexcacheClusterCommand(cc, "SET %s %s", key, value);
    assert(reply != NULL);
    freeReplyObject(reply);

    nexcacheClusterFree(cc);
}

int main(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    options.event_callback = eventCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    if (acc == NULL || acc->err != 0) {
        printf("Connect error: %s\n", acc ? acc->errstr : "OOM");
        exit(2);
    }

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
    return 0;
}
