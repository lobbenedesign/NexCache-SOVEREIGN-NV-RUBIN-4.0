#include "adapters/libevent.h"
#include "cluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"
#define CLUSTER_NODE_WITH_PASSWORD "127.0.0.1:7100"
#define CLUSTER_USERNAME "default"
#define CLUSTER_PASSWORD "secretword"

int connect_success_counter;
int connect_failure_counter;
void connect_callback(const nexcacheContext *c, int status) {
    (void)c;
    if (status == NEXCACHE_OK)
        connect_success_counter++;
    else
        connect_failure_counter++;
}
void reset_counters(void) {
    connect_success_counter = connect_failure_counter = 0;
}

/* Creating a context using unsupported client options should give errors */
void test_unsupported_option(void) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_PREFER_IPV4;
    options.options |= NEXCACHE_OPT_NONBLOCK; /* unsupported in cluster */

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    assert(cc);
    assert(strcmp(cc->errstr, "Unsupported options") == 0);

    nexcacheClusterFree(cc);
}

// Connecting to a password protected cluster and
// providing a correct password.
void test_password_ok(void) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.password = CLUSTER_PASSWORD;
    options.connect_callback = connect_callback;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");
    assert(connect_success_counter == 1); // for CLUSTER SLOTS
    load_nexcache_version(cc);
    // Check that the initial slotmap update connection is reused.
    assert(connect_success_counter == 1);

    // Test connection
    nexcacheReply *reply;
    reply = nexcacheClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);
    nexcacheClusterFree(cc);

    // Check counters incremented by connect callback
    assert(connect_success_counter == 2); // for SET (to a different node)
    assert(connect_failure_counter == 0);
    reset_counters();
}

// Connecting to a password protected cluster and
// providing wrong password.
void test_password_wrong(void) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.password = "faultypass";

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    assert(cc);

    assert(cc->err == NEXCACHE_ERR_OTHER);
    if (nexcache_version_less_than(6, 0))
        assert(strcmp(cc->errstr, "ERR invalid password") == 0);
    else
        assert(strncmp(cc->errstr, "WRONGPASS", 9) == 0);

    nexcacheClusterFree(cc);
}

// Connecting to a password protected cluster and
// not providing any password.
void test_password_missing(void) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    // No password set.

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    assert(cc);

    assert(cc->err == NEXCACHE_ERR_OTHER);
    assert(strncmp(cc->errstr, "NOAUTH", 6) == 0);

    nexcacheClusterFree(cc);
}

// Connect to a cluster and authenticate using username and password,
// i.e. 'AUTH <username> <password>'
void test_username_ok(void) {
    if (nexcache_version_less_than(6, 0))
        return;

    // Connect to the cluster using username and password
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.username = CLUSTER_USERNAME;
    options.password = CLUSTER_PASSWORD;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");

    // Test connection
    nexcacheReply *reply = nexcacheClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    nexcacheClusterFree(cc);
}

// Connect and handle two clusters simultaneously
void test_multicluster(void) {
    nexcacheReply *reply;

    // Connect to first cluster
    nexcacheClusterContext *cc1 = nexcacheClusterConnect(CLUSTER_NODE);
    assert(cc1);
    ASSERT_MSG(cc1->err == 0, cc1->errstr);

    // Connect to second cluster
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.password = CLUSTER_PASSWORD;
    nexcacheClusterContext *cc2 = nexcacheClusterConnectWithOptions(&options);
    assert(cc2);
    ASSERT_MSG(cc2->err == 0, cc2->errstr);

    // Set keys differently in clusters
    reply = nexcacheClusterCommand(cc1, "SET key Hello1");
    CHECK_REPLY_OK(cc1, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc2, "SET key Hello2");
    CHECK_REPLY_OK(cc2, reply);
    freeReplyObject(reply);

    // Verify keys in clusters
    reply = nexcacheClusterCommand(cc1, "GET key");
    CHECK_REPLY_STR(cc1, reply, "Hello1");
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc2, "GET key");
    CHECK_REPLY_STR(cc2, reply, "Hello2");
    freeReplyObject(reply);

    // Disconnect from first cluster
    nexcacheClusterFree(cc1);

    // Verify that key is still accessible in connected cluster
    reply = nexcacheClusterCommand(cc2, "GET key");
    CHECK_REPLY_STR(cc2, reply, "Hello2");
    freeReplyObject(reply);

    nexcacheClusterFree(cc2);
}

/* Connect to a non-routable address which results in a connection timeout. */
void test_connect_timeout(void) {
    struct timeval timeout = {0, 200000};

    /* Configure a non-routable IP address and a timeout */
    nexcacheClusterOptions options = {0};
    options.initial_nodes = "192.168.0.0:7000";
    options.connect_timeout = &timeout;
    options.connect_callback = connect_callback;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    assert(cc);
    assert(cc->err == NEXCACHE_ERR_IO);
    assert(strcmp(cc->errstr, "Connection timed out") == 0);
    assert(connect_success_counter == 0);
    assert(connect_failure_counter == 1);
    reset_counters();

    nexcacheClusterFree(cc);
}

/* Connect using a pre-configured command timeout */
void test_command_timeout(void) {
    struct timeval timeout = {0, 10000};

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.command_timeout = &timeout;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    /* Simulate a command timeout */
    nexcacheReply *reply;
    reply = nexcacheClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    assert(reply == NULL);
    assert(cc->err == NEXCACHE_ERR_IO);

    /* Make sure debug sleep is done before leaving testcase */
    for (int i = 0; i < 20; ++i) {
        reply = nexcacheClusterCommandToNode(cc, node, "SET key1 Hello");
        if (reply && reply->type == NEXCACHE_REPLY_STATUS)
            break;
    }
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    nexcacheClusterFree(cc);
}

/* Connect and configure a command timeout while connected. */
void test_command_timeout_set_while_connected(void) {
    nexcacheClusterContext *cc = nexcacheClusterConnect(CLUSTER_NODE);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    nexcacheReply *reply;
    reply = nexcacheClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    /* Set command timeout while connected */
    struct timeval timeout = {0, 10000};
    nexcacheClusterSetOptionTimeout(cc, timeout);

    reply = nexcacheClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    assert(reply == NULL);
    assert(cc->err == NEXCACHE_ERR_IO);

    /* Make sure debug sleep is done before leaving testcase */
    for (int i = 0; i < 20; ++i) {
        reply = nexcacheClusterCommandToNode(cc, node, "SET key1 Hello");
        if (reply && reply->type == NEXCACHE_REPLY_STATUS)
            break;
    }
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    nexcacheClusterFree(cc);
}

//------------------------------------------------------------------------------
// Async API
//------------------------------------------------------------------------------
typedef struct ExpectedResult {
    int type;
    const char *str;
    bool disconnect;
    bool noreply;
    const char *errstr;
} ExpectedResult;

// Callback for NexCache connects and disconnects
void connectCallback(nexcacheAsyncContext *ac, int status) {
    UNUSED(ac);
    assert(status == NEXCACHE_OK);
}
void disconnectCallback(const nexcacheAsyncContext *ac, int status) {
    UNUSED(ac);
    assert(status == NEXCACHE_OK);
}

// Callback for async commands, verifies the nexcacheReply
void commandCallback(nexcacheClusterAsyncContext *cc, void *r, void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    ExpectedResult *expect = (ExpectedResult *)privdata;
    if (expect->noreply) {
        assert(reply == NULL);
        assert(strcmp(cc->errstr, expect->errstr) == 0);
    } else {
        assert(reply != NULL);
        assert(reply->type == expect->type);
        if (reply->type == NEXCACHE_REPLY_ERROR ||
            reply->type == NEXCACHE_REPLY_STATUS ||
            reply->type == NEXCACHE_REPLY_STRING ||
            reply->type == NEXCACHE_REPLY_DOUBLE ||
            reply->type == NEXCACHE_REPLY_VERB) {
            assert(strcmp(reply->str, expect->str) == 0);
        }
    }
    if (expect->disconnect)
        nexcacheClusterAsyncDisconnect(cc);
}

// Connecting to a password protected cluster using
// the async API, providing correct password.
void test_async_password_ok(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.password = CLUSTER_PASSWORD;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    // Test connection
    ExpectedResult r = {
        .type = NEXCACHE_REPLY_STATUS, .str = "OK", .disconnect = true};
    int ret = nexcacheClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == NEXCACHE_OK);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

/* Connect to a password protected cluster using the wrong password.
   An eventloop is not attached since it is not needed is this case. */
void test_async_password_wrong(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.password = "faultypass";
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    assert(acc->err == NEXCACHE_ERR_OTHER);
    if (nexcache_version_less_than(6, 0))
        assert(strcmp(acc->errstr, "ERR invalid password") == 0);
    else
        assert(strncmp(acc->errstr, "WRONGPASS", 9) == 0);

    // No connection
    ExpectedResult r;
    int ret = nexcacheClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == NEXCACHE_ERR);
    assert(acc->err == NEXCACHE_ERR_OTHER);
    assert(strcmp(acc->errstr, "slotmap not available") == 0);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

/* Connect to a password protected cluster without providing a password.
   An eventloop is not attached since it is not needed is this case. */
void test_async_password_missing(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);
    // Password not configured

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    assert(acc->err == NEXCACHE_ERR_OTHER);
    assert(strncmp(acc->errstr, "NOAUTH", 6) == 0);

    // No connection
    ExpectedResult r;
    int ret = nexcacheClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == NEXCACHE_ERR);
    assert(acc->err == NEXCACHE_ERR_OTHER);
    assert(strcmp(acc->errstr, "slotmap not available") == 0);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

// Connect to a cluster and authenticate using username and password
void test_async_username_ok(void) {
    if (nexcache_version_less_than(6, 0))
        return;
    struct event_base *base = event_base_new();

    // Connect to the cluster using username and password
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    options.username = "missing-user";
    options.password = CLUSTER_PASSWORD;
    nexcacheClusterOptionsUseLibevent(&options, base);

    // Connect using wrong username should fail
    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    assert(acc->err == NEXCACHE_ERR_OTHER);
    assert(strncmp(acc->errstr, "WRONGPASS invalid username-password pair",
                   40) == 0);
    nexcacheClusterAsyncFree(acc);

    // Set correct username
    options.username = CLUSTER_USERNAME;

    // Connect using correct username should pass
    acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    assert(acc->err == 0);

    // Test connection
    ExpectedResult r = {
        .type = NEXCACHE_REPLY_STATUS, .str = "OK", .disconnect = true};
    int ret = nexcacheClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == NEXCACHE_OK);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

// Connect and handle two clusters simultaneously using the async API
void test_async_multicluster(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options1 = {0};
    options1.initial_nodes = CLUSTER_NODE;
    options1.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options1.async_connect_callback = connectCallback;
    options1.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options1, base);

    // Connect to first cluster
    nexcacheClusterAsyncContext *acc1 = nexcacheClusterAsyncConnectWithOptions(&options1);
    ASSERT_MSG(acc1 && acc1->err == 0, acc1 ? acc1->errstr : "OOM");

    nexcacheClusterOptions options2 = {0};
    options2.initial_nodes = CLUSTER_NODE_WITH_PASSWORD;
    options2.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options2.password = CLUSTER_PASSWORD;
    options2.async_connect_callback = connectCallback;
    options2.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options2, base);

    // Connect to second cluster
    nexcacheClusterAsyncContext *acc2 = nexcacheClusterAsyncConnectWithOptions(&options2);
    ASSERT_MSG(acc2 && acc2->err == 0, acc2 ? acc2->errstr : "OOM");

    // Set keys differently in clusters
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_STATUS, .str = "OK"};
    int ret = nexcacheClusterAsyncCommand(acc1, commandCallback, &r1, "SET key A");
    assert(ret == NEXCACHE_OK);

    ExpectedResult r2 = {.type = NEXCACHE_REPLY_STATUS, .str = "OK"};
    ret = nexcacheClusterAsyncCommand(acc2, commandCallback, &r2, "SET key B");
    assert(ret == NEXCACHE_OK);

    // Verify key in first cluster
    ExpectedResult r3 = {.type = NEXCACHE_REPLY_STRING, .str = "A"};
    ret = nexcacheClusterAsyncCommand(acc1, commandCallback, &r3, "GET key");
    assert(ret == NEXCACHE_OK);

    // Verify key in second cluster and disconnect
    ExpectedResult r4 = {
        .type = NEXCACHE_REPLY_STRING, .str = "B", .disconnect = true};
    ret = nexcacheClusterAsyncCommand(acc2, commandCallback, &r4, "GET key");
    assert(ret == NEXCACHE_OK);

    // Verify that key is still accessible in connected cluster
    ExpectedResult r5 = {
        .type = NEXCACHE_REPLY_STRING, .str = "A", .disconnect = true};
    ret = nexcacheClusterAsyncCommand(acc1, commandCallback, &r5, "GET key");
    assert(ret == NEXCACHE_OK);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc1);
    nexcacheClusterAsyncFree(acc2);
    event_base_free(base);
}

/* Connect to a non-routable address which results in a connection timeout. */
void test_async_connect_timeout(void) {
    struct event_base *base = event_base_new();
    struct timeval timeout = {0, 200000};

    nexcacheClusterOptions options = {0};
    /* Configure a non-routable IP address and a timeout */
    options.initial_nodes = "192.168.0.0:7000";
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.connect_timeout = &timeout;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    assert(acc);
    assert(acc->err == NEXCACHE_ERR_IO);
    assert(strcmp(acc->errstr, "Connection timed out") == 0);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

/* Connect using a pre-configured command timeout */
void test_async_command_timeout(void) {
    struct event_base *base = event_base_new();
    struct timeval timeout = {0, 10000};

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.command_timeout = &timeout;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, &acc->cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    /* Simulate a command timeout and expect a timeout error */
    ExpectedResult r = {
        .noreply = true, .errstr = "Timeout", .disconnect = true};
    int status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r,
                                                 "DEBUG SLEEP 0.2");
    assert(status == NEXCACHE_OK);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

int main(void) {

    test_unsupported_option();

    test_password_ok();
    test_password_wrong();
    test_password_missing();
    test_username_ok();
    test_multicluster();
    test_connect_timeout();
    test_command_timeout();
    test_command_timeout_set_while_connected();

    test_async_password_ok();
    test_async_password_wrong();
    test_async_password_missing();
    test_async_username_ok();
    test_async_multicluster();
    test_async_connect_timeout();
    test_async_command_timeout();

    return 0;
}
