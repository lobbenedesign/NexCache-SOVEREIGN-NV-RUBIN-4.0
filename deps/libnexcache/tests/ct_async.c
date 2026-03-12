#include "adapters/libevent.h"
#include "cluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void getCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    nexcacheReply *reply = (nexcacheReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);

    /* Disconnect after receiving the first reply to GET */
    nexcacheClusterAsyncDisconnect(acc);
}

void setCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    nexcacheReply *reply = (nexcacheReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);
}

void connectCallback(nexcacheAsyncContext *ac, int status) {
    ASSERT_MSG(status == NEXCACHE_OK, ac->errstr);
    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const nexcacheAsyncContext *ac, int status) {
    ASSERT_MSG(status == NEXCACHE_OK, ac->errstr);
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void eventCallback(const nexcacheClusterContext *cc, int event, void *privdata) {
    (void)cc;
    (void)privdata;
    /* Get the async context by a simple cast since in the Async API a
     * nexcacheClusterAsyncContext is an extension of the nexcacheClusterContext. */
    nexcacheClusterAsyncContext *acc = (nexcacheClusterAsyncContext *)cc;

    /* We send our commands when the client is ready to accept commands. */
    if (event == VALKEYCLUSTER_EVENT_READY) {
        int status;
        status = nexcacheClusterAsyncCommand(acc, setCallback, (char *)"ID",
                                           "SET key12345 value");
        ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

        /* This command will trigger a disconnect in its reply callback. */
        status = nexcacheClusterAsyncCommand(acc, getCallback, (char *)"ID",
                                           "GET key12345");
        ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

        status = nexcacheClusterAsyncCommand(acc, setCallback, (char *)"ID",
                                           "SET key23456 value2");
        ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

        status = nexcacheClusterAsyncCommand(acc, getCallback, (char *)"ID",
                                           "GET key23456");
        ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);
    }
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
    assert(acc && acc->err == 0);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
    return 0;
}
