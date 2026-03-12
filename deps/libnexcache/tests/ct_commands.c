#define _XOPEN_SOURCE 600 /* For strdup() */
#include "cluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void test_exists(nexcacheClusterContext *cc) {
    nexcacheReply *reply;
    reply = nexcacheClusterCommand(cc, "SET {key}1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "EXISTS {key}1");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "EXISTS nosuch{key}");
    CHECK_REPLY_INT(cc, reply, 0);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "SET {key}2 World");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "EXISTS {key}1 {key}2 nosuch{key}");
    CHECK_REPLY_INT(cc, reply, 2);
    freeReplyObject(reply);
}

void test_bitfield(nexcacheClusterContext *cc) {
    nexcacheReply *reply;

    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "BITFIELD bkey1 SET u32 #0 255 GET u32 #0");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_INT(cc, reply->element[1], 255);
    freeReplyObject(reply);
}

void test_bitfield_ro(nexcacheClusterContext *cc) {
    if (nexcache_version_less_than(6, 0))
        return; /* Skip test, command not available. */

    nexcacheReply *reply;

    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "SET bkey2 a"); // 97
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply =
        (nexcacheReply *)nexcacheClusterCommand(cc, "BITFIELD_RO bkey2 GET u8 #0");
    CHECK_REPLY_ARRAY(cc, reply, 1);
    CHECK_REPLY_INT(cc, reply->element[0], 97);
    freeReplyObject(reply);
}

void test_mset(nexcacheClusterContext *cc) {
    nexcacheReply *reply;
    reply =
        nexcacheClusterCommand(cc, "MSET {key}1 mset1 {key}2 mset2 {key}3 mset3");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "GET {key}1");
    CHECK_REPLY_STR(cc, reply, "mset1");
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "GET {key}2");
    CHECK_REPLY_STR(cc, reply, "mset2");
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "GET {key}3");
    CHECK_REPLY_STR(cc, reply, "mset3");
    freeReplyObject(reply);
}

void test_mget(nexcacheClusterContext *cc) {
    nexcacheReply *reply;
    reply = nexcacheClusterCommand(cc, "SET {key}1 mget1");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "SET {key}2 mget2");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "SET {key}3 mget3");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = nexcacheClusterCommand(cc, "MGET {key}1 {key}2 {key}3");
    CHECK_REPLY_ARRAY(cc, reply, 3);
    CHECK_REPLY_STR(cc, reply->element[0], "mget1");
    CHECK_REPLY_STR(cc, reply->element[1], "mget2");
    CHECK_REPLY_STR(cc, reply->element[2], "mget3");
    freeReplyObject(reply);
}

void test_hset_hget_hdel_hexists(nexcacheClusterContext *cc) {
    nexcacheReply *reply;

    // Prepare
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HDEL myhash field1");
    CHECK_REPLY(cc, reply);
    freeReplyObject(reply);
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HDEL myhash field2");
    CHECK_REPLY(cc, reply);
    freeReplyObject(reply);

    // Set hash field
    reply =
        (nexcacheReply *)nexcacheClusterCommand(cc, "HSET myhash field1 hsetvalue");
    CHECK_REPLY_INT(cc, reply, 1); // Set 1 field
    freeReplyObject(reply);

    // Set second hash field
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "HSET myhash field3 hsetvalue3");
    CHECK_REPLY_INT(cc, reply, 1); // Set 1 field
    freeReplyObject(reply);

    // Get field value
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HGET myhash field1");
    CHECK_REPLY_STR(cc, reply, "hsetvalue");
    freeReplyObject(reply);

    // Get field that is not present
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HGET myhash field2");
    CHECK_REPLY_NIL(cc, reply);
    freeReplyObject(reply);

    // Delete a field
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HDEL myhash field1");
    CHECK_REPLY_INT(cc, reply, 1); // Delete 1 field
    freeReplyObject(reply);

    // Delete a field that is not present
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HDEL myhash field2");
    CHECK_REPLY_INT(cc, reply, 0); // Nothing to delete
    freeReplyObject(reply);

    // Check if field exists
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HEXISTS myhash field3");
    CHECK_REPLY_INT(cc, reply, 1); // exists
    freeReplyObject(reply);

    // Delete multiple fields at once
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "HDEL myhash field1 field2 field3");
    CHECK_REPLY_INT(cc, reply, 1); // field3 deleted
    freeReplyObject(reply);

    // Make sure field3 is deleted now
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "HEXISTS myhash field3");
    CHECK_REPLY_INT(cc, reply, 0); // no field
    freeReplyObject(reply);

    // Set multiple fields at once
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "HSET myhash field1 hsetvalue1 field2 hsetvalue2");
    CHECK_REPLY_INT(cc, reply, 2);
    freeReplyObject(reply);
}

// Command layout:
// eval <script> <no of keys> <keys..> <args..>
void test_eval(nexcacheClusterContext *cc) {
    nexcacheReply *reply;

    // Single key
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 1 %s", "return nexcache.call('set',KEYS[1],'bar')", "foo");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    // Single key, string response
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "eval %s 1 %s",
                                                "return KEYS[1]", "key1");
    CHECK_REPLY_STR(cc, reply, "key1");
    freeReplyObject(reply);

    // Single key with single argument
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 1 %s %s", "return {KEYS[1],ARGV[1]}", "key1", "first");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_STR(cc, reply->element[0], "key1");
    CHECK_REPLY_STR(cc, reply->element[1], "first");
    freeReplyObject(reply);

    // Multi key, but handled by same instance
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 2 %s %s", "return {KEYS[1],KEYS[2]}", "key1", "key1");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_STR(cc, reply->element[0], "key1");
    CHECK_REPLY_STR(cc, reply->element[1], "key1");
    freeReplyObject(reply);

    // Error handling in EVAL

    // Request without key, fails due to no key given.
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 0", "return nexcache.call('set','foo','bar')");
    assert(reply == NULL);

    // Prepare a key to be a list-type, then run a script the attempts
    // to access it as a simple type.
    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "del foo");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = (nexcacheReply *)nexcacheClusterCommand(cc, "lpush foo a");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 1 %s", "return nexcache.call('get',KEYS[1])", "foo");
    if (nexcache_version_less_than(7, 0)) {
        CHECK_REPLY_ERROR(cc, reply, "ERR Error running script");
    } else {
        CHECK_REPLY_ERROR(cc, reply, "WRONGTYPE");
    }
    freeReplyObject(reply);

    // Two keys handled by different instances,
    // will fail due to CROSSSLOT.
    reply = (nexcacheReply *)nexcacheClusterCommand(
        cc, "eval %s 2 %s %s %s %s", "return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}",
        "key1", "key2", "first", "second");
    CHECK_REPLY_ERROR(cc, reply, "CROSSSLOT");
    freeReplyObject(reply);
}

void test_xack(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    /* Prepare a stream and group */
    r = nexcacheClusterCommand(cc, "XADD mystream * field1 value1");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_STRING);
    freeReplyObject(r);
    r = nexcacheClusterCommand(cc, "XGROUP DESTROY mystream mygroup");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);
    r = nexcacheClusterCommand(cc, "XGROUP CREATE mystream mygroup 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XACK mystream mygroup 1526569495631-0");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);
}

void test_xadd(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(cc, "XADD mystream * field1 value1");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_STRING);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XADD mystream * field1 value1 field2 value2");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_STRING);
    freeReplyObject(r);

    r = nexcacheClusterCommand(
        cc, "XADD mystream * field1 value1 field2 value2 field3 value3");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_STRING);
    freeReplyObject(r);
}

void test_xautoclaim(nexcacheClusterContext *cc) {
    if (nexcache_version_less_than(6, 2))
        return; /* Skip test, command not available. */

    nexcacheReply *r;

    r = nexcacheClusterCommand(
        cc, "XAUTOCLAIM mystream mygroup Alice 3600000 0-0 COUNT 25");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xclaim(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(
        cc, "XCLAIM mystream mygroup Alice 3600000 1526569498055-0");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xdel(nexcacheClusterContext *cc) {
    nexcacheReply *r;
    char *id;

    r = nexcacheClusterCommand(cc, "XADD mystream * field value");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_STRING);
    id = strdup(r->str); /* Keep the id */
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XDEL mystream %s", id);
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);

    /* Verify client handling of multiple id values / arguments */
    r = nexcacheClusterCommand(cc, "XDEL mystream %s %s", id, id);
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);

    free(id);
}

void test_xgroup(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    /* Test of missing subcommand */
    r = nexcacheClusterCommand(cc, "XGROUP");
    assert(r == NULL);

    /* Test of missing stream/key */
    r = nexcacheClusterCommand(cc, "XGROUP CREATE");
    assert(r == NULL);

    /* Test the destroy command first as preparation */
    r = nexcacheClusterCommand(cc, "XGROUP DESTROY mystream consumer-group-name");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc,
                             "XGROUP CREATE mystream consumer-group-name 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);

    /* Attempting to create an already existing group gives error */
    r = nexcacheClusterCommand(cc,
                             "XGROUP CREATE mystream consumer-group-name 0");
    CHECK_REPLY_ERROR(cc, r, "BUSYGROUP");
    freeReplyObject(r);

    if (!nexcache_version_less_than(6, 2)) {
        /* Test of subcommand CREATECONSUMER when available. */
        r = nexcacheClusterCommand(
            cc,
            "XGROUP CREATECONSUMER mystream consumer-group-name myconsumer123");
        CHECK_REPLY_INT(cc, r, 1);
        freeReplyObject(r);
    }

    r = nexcacheClusterCommand(
        cc, "XGROUP DELCONSUMER mystream consumer-group-name myconsumer123");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XGROUP SETID mystream consumer-group-name 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);
}

void test_xinfo(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    /* Test of missing subcommand */
    r = nexcacheClusterCommand(cc, "XINFO");
    assert(r == NULL);

    /* Test of missing stream/key */
    r = nexcacheClusterCommand(cc, "XINFO STREAM");
    assert(r == NULL);

    r = nexcacheClusterCommand(cc, "XINFO STREAM mystream");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);

    if (!nexcache_version_less_than(6, 0)) {
        /* Test of subcommand STREAM with arguments when available. */
        r = nexcacheClusterCommand(cc, "XINFO STREAM mystream FULL COUNT 1");
        CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
        freeReplyObject(r);
    }

    r = nexcacheClusterCommand(cc, "XINFO GROUPS mystream");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc,
                             "XINFO CONSUMERS mystream consumer-group-name");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xlen(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = (nexcacheReply *)nexcacheClusterCommand(cc, "XLEN mystream");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);
}

void test_xpending(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(cc, "XPENDING mystream mygroup");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XPENDING mystream mygroup - + 10");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xrange(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(cc, "XRANGE mystream 0 0");
    CHECK_REPLY_ARRAY(cc, r, 0); /* No entries due to 0-range */
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XRANGE mystream - + COUNT 1");
    CHECK_REPLY_ARRAY(cc, r, 1); /* Single entry due to count argument */
    freeReplyObject(r);
}

void test_xrevrange(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(cc, "XREVRANGE mystream 0 0");
    CHECK_REPLY_ARRAY(cc, r, 0); /* No entries due to 0-range */
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XREVRANGE mystream + - COUNT 1");
    CHECK_REPLY_ARRAY(cc, r, 1); /* Single entry due to count argument */
    freeReplyObject(r);
}

void test_xtrim(nexcacheClusterContext *cc) {
    nexcacheReply *r;

    r = nexcacheClusterCommand(cc, "XTRIM mystream MAXLEN 200");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);

    r = nexcacheClusterCommand(cc, "XTRIM mystream MAXLEN ~ 100");
    CHECK_REPLY_TYPE(r, NEXCACHE_REPLY_INTEGER);
    freeReplyObject(r);
}

void test_multi(nexcacheClusterContext *cc) {

    /* Since the slot lookup is currently not handled for this command
     * the regular API is expected to fail. This command requires the
     * ..ToNode() APIs for sending a command to a specific node.
     * See ct_specific_nodes.c for these tests.
     */

    nexcacheReply *r = nexcacheClusterCommand(cc, "MULTI");
    assert(r == NULL);
}

int main(void) {
    struct timeval timeout = {0, 500000};

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE;
    options.connect_timeout = &timeout;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    ASSERT_MSG(cc && cc->err == 0, cc ? cc->errstr : "OOM");
    load_nexcache_version(cc);

    test_bitfield(cc);
    test_bitfield_ro(cc);
    test_eval(cc);
    test_exists(cc);
    test_hset_hget_hdel_hexists(cc);
    test_mget(cc);
    test_mset(cc);
    test_multi(cc);
    test_xack(cc);
    test_xadd(cc);
    test_xautoclaim(cc);
    test_xclaim(cc);
    test_xdel(cc);
    test_xgroup(cc);
    test_xinfo(cc);
    test_xlen(cc);
    test_xpending(cc);
    test_xrange(cc);
    test_xrevrange(cc);
    test_xtrim(cc);

    nexcacheClusterFree(cc);
    return 0;
}
