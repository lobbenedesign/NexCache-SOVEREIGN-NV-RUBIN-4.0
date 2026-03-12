#include <nexcache/tls.h>
#include <nexcache/nexcache.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <winsock2.h> /* For struct timeval */
#endif

int main(int argc, char **argv) {
    unsigned int j;
    nexcacheTLSContext *tls;
    nexcacheTLSContextError tls_error = NEXCACHE_TLS_CTX_NONE;
    nexcacheContext *c;
    nexcacheReply *reply;
    if (argc < 4) {
        printf("Usage: %s <host> <port> <cert> <key> [ca]\n", argv[0]);
        exit(1);
    }
    const char *hostname = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = atoi(argv[2]);
    const char *cert = argv[3];
    const char *key = argv[4];
    const char *ca = argc > 4 ? argv[5] : NULL;

    nexcacheInitOpenSSL();
    tls = nexcacheCreateTLSContext(ca, NULL, cert, key, NULL, &tls_error);
    if (!tls || tls_error != NEXCACHE_TLS_CTX_NONE) {
        printf("TLS Context error: %s\n", nexcacheTLSContextGetError(tls_error));
        exit(1);
    }

    struct timeval tv = {1, 500000}; // 1.5 seconds
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, hostname, port);
    options.connect_timeout = &tv;
    c = nexcacheConnectWithOptions(&options);

    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            nexcacheFree(c);
        } else {
            printf("Connection error: can't allocate nexcache context\n");
        }
        exit(1);
    }

    if (nexcacheInitiateTLSWithContext(c, tls) != NEXCACHE_OK) {
        printf("Couldn't initialize TLS!\n");
        printf("Error: %s\n", c->errstr);
        nexcacheFree(c);
        exit(1);
    }

    /* PING server */
    reply = nexcacheCommand(c, "PING");
    printf("PING: %s\n", reply->str);
    freeReplyObject(reply);

    /* Set a key */
    reply = nexcacheCommand(c, "SET %s %s", "foo", "hello world");
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    /* Set a key using binary safe API */
    reply = nexcacheCommand(c, "SET %b %b", "bar", (size_t)3, "hello", (size_t)5);
    printf("SET (binary API): %s\n", reply->str);
    freeReplyObject(reply);

    /* Try a GET and two INCR */
    reply = nexcacheCommand(c, "GET foo");
    printf("GET foo: %s\n", reply->str);
    freeReplyObject(reply);

    reply = nexcacheCommand(c, "INCR counter");
    printf("INCR counter: %lld\n", reply->integer);
    freeReplyObject(reply);
    /* again ... */
    reply = nexcacheCommand(c, "INCR counter");
    printf("INCR counter: %lld\n", reply->integer);
    freeReplyObject(reply);

    /* Create a list of numbers, from 0 to 9 */
    reply = nexcacheCommand(c, "DEL mylist");
    freeReplyObject(reply);
    for (j = 0; j < 10; j++) {
        char buf[64];

        snprintf(buf, 64, "%u", j);
        reply = nexcacheCommand(c, "LPUSH mylist element-%s", buf);
        freeReplyObject(reply);
    }

    /* Let's check what we have inside the list */
    reply = nexcacheCommand(c, "LRANGE mylist 0 -1");
    if (reply->type == NEXCACHE_REPLY_ARRAY) {
        for (j = 0; j < reply->elements; j++) {
            printf("%u) %s\n", j, reply->element[j]->str);
        }
    }
    freeReplyObject(reply);

    /* Disconnects and frees the context */
    nexcacheFree(c);

    nexcacheFreeTLSContext(tls);

    return 0;
}
