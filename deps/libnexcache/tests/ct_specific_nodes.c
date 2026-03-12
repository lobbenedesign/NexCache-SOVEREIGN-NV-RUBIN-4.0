#define _XOPEN_SOURCE 600 /* For strdup() */
#include "adapters/libevent.h"
#include "cluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void test_command_to_single_node(nexcacheClusterContext *cc) {
    nexcacheReply *reply;

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    reply = nexcacheClusterCommandToNode(cc, node, "DBSIZE");
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);
}

void test_command_to_all_nodes(nexcacheClusterContext *cc) {

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);

    nexcacheClusterNode *node;
    while ((node = nexcacheClusterNodeNext(&ni)) != NULL) {

        nexcacheReply *reply;
        reply = nexcacheClusterCommandToNode(cc, node, "DBSIZE");
        CHECK_REPLY(cc, reply);
        CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
        freeReplyObject(reply);
    }
}

void test_transaction(nexcacheClusterContext *cc) {

    nexcacheClusterNode *node = nexcacheClusterGetNodeByKey(cc, (char *)"foo");
    assert(node);

    nexcacheReply *reply;
    reply = nexcacheClusterCommandToNode(cc, node, "MULTI");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommandToNode(cc, node, "SET foo 99");
    CHECK_REPLY_QUEUED(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommandToNode(cc, node, "INCR foo");
    CHECK_REPLY_QUEUED(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommandToNode(cc, node, "EXEC");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_OK(cc, reply->element[0]);
    CHECK_REPLY_INT(cc, reply->element[1], 100);
    freeReplyObject(reply);
}

void test_streams(nexcacheClusterContext *cc) {
    nexcacheReply *reply;
    char *id;

    /* Get the node that handles given stream */
    nexcacheClusterNode *node = nexcacheClusterGetNodeByKey(cc, (char *)"mystream");
    assert(node);

    /* Preparation: remove old stream/key */
    reply = nexcacheClusterCommandToNode(cc, node, "DEL mystream");
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);

    /* Query wrong node */
    nexcacheClusterNode *wrongNode = nexcacheClusterGetNodeByKey(cc, (char *)"otherstream");
    assert(node != wrongNode);
    reply = nexcacheClusterCommandToNode(cc, wrongNode, "XLEN mystream");
    CHECK_REPLY_ERROR(cc, reply, "MOVED");
    freeReplyObject(reply);

    /* Verify stream length before adding entries */
    reply = nexcacheClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 0);
    freeReplyObject(reply);

    /* Add entries to a created stream */
    reply = nexcacheClusterCommandToNode(cc, node, "XADD mystream * name t800");
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_STRING);
    freeReplyObject(reply);

    reply = nexcacheClusterCommandToNode(
        cc, node, "XADD mystream * name Sara surname OConnor");
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_STRING);
    id = strdup(reply->str); /* Keep this id for later inspections */
    freeReplyObject(reply);

    /* Verify stream length after adding entries */
    reply = nexcacheClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 2);
    freeReplyObject(reply);

    /* Modify the stream */
    reply = nexcacheClusterCommandToNode(cc, node, "XTRIM mystream MAXLEN 1");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    /* Verify stream length after modifying the stream */
    reply = nexcacheClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 1); /* 1 entry left */
    freeReplyObject(reply);

    /* Read from the stream */
    reply = nexcacheClusterCommandToNode(cc, node,
                                       "XREAD COUNT 2 STREAMS mystream 0");
    CHECK_REPLY_ARRAY(cc, reply, 1); /* Reply from a single stream */

    /* Verify the reply from stream */
    CHECK_REPLY_ARRAY(cc, reply->element[0], 2);
    CHECK_REPLY_STR(cc, reply->element[0]->element[0], "mystream");
    CHECK_REPLY_ARRAY(cc, reply->element[0]->element[1], 1); /* single entry */

    /* Verify the entry, an array of id and field+value elements */
    nexcacheReply *entry = reply->element[0]->element[1]->element[0];
    CHECK_REPLY_ARRAY(cc, entry, 2);
    CHECK_REPLY_STR(cc, entry->element[0], id);

    CHECK_REPLY_ARRAY(cc, entry->element[1], 4);
    CHECK_REPLY_STR(cc, entry->element[1]->element[0], "name");
    CHECK_REPLY_STR(cc, entry->element[1]->element[1], "Sara");
    CHECK_REPLY_STR(cc, entry->element[1]->element[2], "surname");
    CHECK_REPLY_STR(cc, entry->element[1]->element[3], "OConnor");
    freeReplyObject(reply);

    /* Delete the entry in stream */
    reply = nexcacheClusterCommandToNode(cc, node, "XDEL mystream %s", id);
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    /* Blocking read of stream */
    reply = nexcacheClusterCommandToNode(
        cc, node, "XREAD COUNT 2 BLOCK 200 STREAMS mystream 0");
    CHECK_REPLY_NIL(cc, reply);
    freeReplyObject(reply);

    /* Create a consumer group */
    reply = nexcacheClusterCommandToNode(cc, node,
                                       "XGROUP CREATE mystream mygroup1 0");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    if (!nexcache_version_less_than(6, 2)) {
        /* Create a consumer */
        reply = nexcacheClusterCommandToNode(
            cc, node, "XGROUP CREATECONSUMER mystream mygroup1 myconsumer123");
        CHECK_REPLY_INT(cc, reply, 1);
        freeReplyObject(reply);
    }

    /* Blocking read of consumer group */
    reply =
        nexcacheClusterCommandToNode(cc, node,
                                   "XREADGROUP GROUP mygroup1 myconsumer123 "
                                   "COUNT 2 BLOCK 200 STREAMS mystream 0");
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(reply);

    free(id);
}

void test_pipeline_to_single_node(nexcacheClusterContext *cc) {
    int status;
    nexcacheReply *reply;

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    status = nexcacheClusterAppendCommandToNode(cc, node, "DBSIZE");
    ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);

    // Trigger send of pipeline commands
    nexcacheClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);
}

void test_pipeline_to_all_nodes(nexcacheClusterContext *cc) {

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, cc);

    nexcacheClusterNode *node;
    while ((node = nexcacheClusterNodeNext(&ni)) != NULL) {
        int status = nexcacheClusterAppendCommandToNode(cc, node, "DBSIZE");
        ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);
    }

    // Get replies from 3 node cluster
    nexcacheReply *reply;
    nexcacheClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);

    nexcacheClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);

    nexcacheClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(reply);

    nexcacheClusterGetReply(cc, (void *)&reply);
    assert(reply == NULL);
}

void test_pipeline_transaction(nexcacheClusterContext *cc) {
    int status;
    nexcacheReply *reply;

    nexcacheClusterNode *node = nexcacheClusterGetNodeByKey(cc, (char *)"foo");
    assert(node);

    status = nexcacheClusterAppendCommandToNode(cc, node, "MULTI");
    ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);
    status = nexcacheClusterAppendCommandToNode(cc, node, "SET foo 199");
    ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);
    status = nexcacheClusterAppendCommandToNode(cc, node, "INCR foo");
    ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);
    status = nexcacheClusterAppendCommandToNode(cc, node, "EXEC");
    ASSERT_MSG(status == NEXCACHE_OK, cc->errstr);

    // Trigger send of pipeline commands
    {
        nexcacheClusterGetReply(cc, (void *)&reply); // MULTI
        CHECK_REPLY_OK(cc, reply);
        freeReplyObject(reply);

        nexcacheClusterGetReply(cc, (void *)&reply); // SET
        CHECK_REPLY_QUEUED(cc, reply);
        freeReplyObject(reply);

        nexcacheClusterGetReply(cc, (void *)&reply); // INCR
        CHECK_REPLY_QUEUED(cc, reply);
        freeReplyObject(reply);

        nexcacheClusterGetReply(cc, (void *)&reply); // EXEC
        CHECK_REPLY_ARRAY(cc, reply, 2);
        CHECK_REPLY_OK(cc, reply->element[0]);
        CHECK_REPLY_INT(cc, reply->element[1], 200);
        freeReplyObject(reply);
    }
}

//------------------------------------------------------------------------------
// Async API
//------------------------------------------------------------------------------
typedef struct ExpectedResult {
    int type;
    const char *str;
    size_t elements;
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
        switch (reply->type) {
        case NEXCACHE_REPLY_ARRAY:
            assert(reply->elements == expect->elements);
            assert(reply->str == NULL);
            break;
        case NEXCACHE_REPLY_INTEGER:
            assert(reply->elements == 0);
            assert(reply->str == NULL);
            break;
        case NEXCACHE_REPLY_STATUS:
            assert(strcmp(reply->str, expect->str) == 0);
            assert(reply->elements == 0);
            break;
        default:
            assert(0);
        }
    }
    if (expect->disconnect)
        nexcacheClusterAsyncDisconnect(cc);
}

void test_async_to_single_node(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.max_retry = 1;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, &acc->cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    int status;
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_INTEGER, .disconnect = true};
    status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r1,
                                             "DBSIZE");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_formatted_to_single_node(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.max_retry = 1;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, &acc->cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    int status;
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_INTEGER, .disconnect = true};
    char command[] = "*1\r\n$6\r\nDBSIZE\r\n";
    status = nexcacheClusterAsyncFormattedCommandToNode(
        acc, node, commandCallback, &r1, command, strlen(command));
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_command_argv_to_single_node(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.max_retry = 1;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, &acc->cc);
    nexcacheClusterNode *node = nexcacheClusterNodeNext(&ni);
    assert(node);

    int status;
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_INTEGER, .disconnect = true};
    status = nexcacheClusterAsyncCommandArgvToNode(
        acc, node, commandCallback, &r1, 1, (const char *[]){"DBSIZE"},
        (size_t[]){6});
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_to_all_nodes(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.max_retry = 1;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNodeIterator ni;
    nexcacheClusterInitNodeIterator(&ni, &acc->cc);

    int status;
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_INTEGER};

    nexcacheClusterNode *node;
    while ((node = nexcacheClusterNodeNext(&ni)) != NULL) {

        status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback,
                                                 &r1, "DBSIZE");
        ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);
    }

    // Normal command to trigger disconnect
    ExpectedResult r2 = {
        .type = NEXCACHE_REPLY_STATUS, .str = "OK", .disconnect = true};
    status =
        nexcacheClusterAsyncCommand(acc, commandCallback, &r2, "SET foo bar");

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_transaction(void) {
    struct event_base *base = event_base_new();

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.options = NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE;
    options.max_retry = 1;
    options.async_connect_callback = connectCallback;
    options.async_disconnect_callback = disconnectCallback;
    nexcacheClusterOptionsUseLibevent(&options, base);

    nexcacheClusterAsyncContext *acc = nexcacheClusterAsyncConnectWithOptions(&options);
    ASSERT_MSG(acc && acc->err == 0, acc ? acc->errstr : "OOM");

    nexcacheClusterNode *node = nexcacheClusterGetNodeByKey(&acc->cc, (char *)"foo");
    assert(node);

    int status;
    ExpectedResult r1 = {.type = NEXCACHE_REPLY_STATUS, .str = "OK"};
    status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r1,
                                             "MULTI");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    ExpectedResult r2 = {.type = NEXCACHE_REPLY_STATUS, .str = "QUEUED"};
    status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r2,
                                             "SET foo 99");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    ExpectedResult r3 = {.type = NEXCACHE_REPLY_STATUS, .str = "QUEUED"};
    status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r3,
                                             "INCR foo");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    /* The EXEC command will return an array with result from 2 queued commands. */
    ExpectedResult r4 = {
        .type = NEXCACHE_REPLY_ARRAY, .elements = 2, .disconnect = true};
    status = nexcacheClusterAsyncCommandToNode(acc, node, commandCallback, &r4,
                                             "EXEC ");
    ASSERT_MSG(status == NEXCACHE_OK, acc->errstr);

    event_base_dispatch(base);

    nexcacheClusterAsyncFree(acc);
    event_base_free(base);
}

int main(void) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.max_retry = 1;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");
    load_nexcache_version(cc);

    // Synchronous API
    test_command_to_single_node(cc);
    test_command_to_all_nodes(cc);
    test_transaction(cc);
    test_streams(cc);

    // Pipeline API
    test_pipeline_to_single_node(cc);
    test_pipeline_to_all_nodes(cc);
    test_pipeline_transaction(cc);

    nexcacheClusterFree(cc);

    // Asynchronous API
    test_async_to_single_node();
    test_async_formatted_to_single_node();
    test_async_command_argv_to_single_node();
    test_async_to_all_nodes();
    test_async_transaction();

    return 0;
}
