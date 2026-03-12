#include <nexcache/cluster.h>
#include <nexcache/tls.h>

#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_NODE_TLS "127.0.0.1:7301"

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

    struct timeval timeout = {1, 500000}; // 1.5s

    nexcacheClusterOptions options = {0};
    options.initial_nodes = CLUSTER_NODE_TLS;
    options.connect_timeout = &timeout;
    options.tls = tls;
    options.tls_init_fn = &nexcacheInitiateTLSWithContext;

    nexcacheClusterContext *cc = nexcacheClusterConnectWithOptions(&options);
    if (!cc) {
        printf("Error: Allocation failure\n");
        exit(-1);
    } else if (cc->err) {
        printf("Error: %s\n", cc->errstr);
        // handle error
        exit(-1);
    }

    nexcacheReply *reply = nexcacheClusterCommand(cc, "SET %s %s", "key", "value");
    if (!reply) {
        printf("Reply missing: %s\n", cc->errstr);
        exit(-1);
    }
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    nexcacheReply *reply2 = nexcacheClusterCommand(cc, "GET %s", "key");
    if (!reply2) {
        printf("Reply missing: %s\n", cc->errstr);
        exit(-1);
    }
    printf("GET: %s\n", reply2->str);
    freeReplyObject(reply2);

    nexcacheClusterFree(cc);
    nexcacheFreeTLSContext(tls);
    return 0;
}
