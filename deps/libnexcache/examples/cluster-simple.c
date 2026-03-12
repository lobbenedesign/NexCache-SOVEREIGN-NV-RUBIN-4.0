#include <nexcache/cluster.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    struct timeval timeout = {1, 500000}; // 1.5s

    nexcacheClusterOptions options = {0};
    options.initial_nodes = "127.0.0.1:7000";
    options.connect_timeout = &timeout;

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
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    nexcacheReply *reply2 = nexcacheClusterCommand(cc, "GET %s", "key");
    printf("GET: %s\n", reply2->str);
    freeReplyObject(reply2);

    nexcacheClusterFree(cc);
    return 0;
}
