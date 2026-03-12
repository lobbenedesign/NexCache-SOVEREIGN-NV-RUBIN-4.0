#include "adapters/glib.h"
#include "cluster.h"
#include "test_utils.h"

#include <assert.h>

#define CLUSTER_NODE "127.0.0.1:7000"

static GMainLoop *mainloop;

void setCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    nexcacheReply *reply = (nexcacheReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);
}

void getCallback(nexcacheClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    nexcacheReply *reply = (nexcacheReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);

    /* Disconnect after receiving the first reply to GET */
    nexcacheClusterAsyncDisconnect(acc);
    g_main_loop_quit(mainloop);
}

void connectCallback(nexcacheAsyncContext *ac, int status) {
    ASSERT_MSG(status == NEXCACHE_OK, ac->errstr);
    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const nexcacheAsyncContext *ac, int status) {
    ASSERT_MSG(status == NEXCACHE_OK, ac->errstr);
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    GMainContext *context = NULL;
    mainloop = g_main_loop_new(context, FALSE);

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseGlib(&options, context);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    ASSERT_MSG(acc->err == 0, acc->errstr);

    int status;
    status = nexcacheClusterAsyncCommand(acc, setCallback, (char *)"id", "SET key value");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    status = nexcacheClusterAsyncCommand(acc, getCallback, (char *)"id", "GET key");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    g_main_loop_run(mainloop);

    nexcacheClusterAsyncFree(acc);
    return 0;
}
