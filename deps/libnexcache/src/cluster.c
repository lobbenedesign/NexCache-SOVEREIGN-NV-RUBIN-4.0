/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020, Nick <heronr1 at gmail dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
 * Copyright (c) 2020-2021, Viktor Söderqvist <viktor.soderqvist at est dot tech>
 * Copyright (c) 2021, Red Hat
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NexCache nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "fmacros.h"
#include "win32.h"

#include "cluster.h"

#include "adlist.h"
#include "alloc.h"
#include "command.h"
#include "vkutil.h"

#include <dict.h>
#include <sds.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Make sure standalone and cluster options don't overlap. */
vk_static_assert(NEXCACHE_OPT_USE_CLUSTER_NODES > NEXCACHE_OPT_LAST_SA_OPTION);

/* Internal option flags. */
#define NEXCACHE_FLAG_USE_CLUSTER_NODES 0x1
#define NEXCACHE_FLAG_PARSE_REPLICAS 0x2
#define NEXCACHE_FLAG_DISCONNECTING 0x4
#define NEXCACHE_FLAG_BLOCKING_INITIAL_UPDATE 0x8

// Cluster errors are offset by 100 to be sufficiently out of range of
// standard NexCache errors
#define NEXCACHE_ERR_CLUSTER_TOO_MANY_RETRIES 100

#define NEXCACHE_COMMAND_CLUSTER_NODES "CLUSTER NODES"
#define NEXCACHE_COMMAND_CLUSTER_SLOTS "CLUSTER SLOTS"
#define NEXCACHE_COMMAND_ASKING "ASKING"

#define CLUSTER_DEFAULT_MAX_RETRY_COUNT 5
#define NO_RETRY -1

#define SLOTMAP_UPDATE_THROTTLE_USEC 1000000
#define SLOTMAP_UPDATE_ONGOING INT64_MAX

typedef struct cluster_async_data {
    nexcacheClusterAsyncContext *acc;
    struct cmd *command;
    nexcacheClusterCallbackFn *callback;
    int retry_count;
    void *privdata;
} cluster_async_data;

typedef enum {
    CLUSTER_NO_ERROR = 0,
    CLUSTER_ERR_MOVED,
    CLUSTER_ERR_ASK,
    CLUSTER_ERR_TRYAGAIN,
    CLUSTER_ERR_CLUSTERDOWN,
    CLUSTER_ERR_OTHER
} replyErrorType;

static void freeNexCacheClusterNode(nexcacheClusterNode *node);
static void cluster_slot_destroy(cluster_slot *slot);
static int updateNodesAndSlotmap(nexcacheClusterContext *cc, dict *nodes);
static int updateSlotMapAsync(nexcacheClusterAsyncContext *acc,
                              nexcacheAsyncContext *ac);
static int nexcacheClusterSetOptionAddNodes(nexcacheClusterContext *cc, const char *addrs);
static int nexcacheClusterSetOptionConnectTimeout(nexcacheClusterContext *cc, const struct timeval tv);
static int nexcacheClusterSetOptionPassword(nexcacheClusterContext *cc, const char *password);
static int nexcacheClusterSetOptionUsername(nexcacheClusterContext *cc, const char *username);
static int nexcacheClusterAsyncConnect(nexcacheClusterAsyncContext *acc);

void listClusterNodeDestructor(void *val) { freeNexCacheClusterNode(val); }

void listClusterSlotDestructor(void *val) { cluster_slot_destroy(val); }

static uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char *)key, sdslen((char *)key));
}

static int dictSdsKeyCompare(const void *key1, const void *key2) {
    int l1, l2;

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2)
        return 0;
    return memcmp(key1, key2, l1) == 0;
}

static void dictSdsDestructor(void *val) {
    sdsfree(val);
}

static void dictClusterNodeDestructor(void *val) {
    freeNexCacheClusterNode(val);
}

/* Destructor function for clusterNodeListDictType. */
static void dictClusterNodeListDestructor(void *val) {
    listRelease(val);
}

/* Cluster node hash table
 * maps node address (1.2.3.4:6379) to a nexcacheClusterNode
 * Has ownership of nexcacheClusterNode memory
 */
dictType clusterNodesDictType = {
    .hashFunction = dictSdsHash,
    .keyCompare = dictSdsKeyCompare,
    .keyDestructor = dictSdsDestructor,
    .valDestructor = dictClusterNodeDestructor};

/* Hash table dictType to map node address to a list of nexcacheClusterNodes. */
dictType clusterNodeListDictType = {
    .hashFunction = dictSdsHash,
    .keyCompare = dictSdsKeyCompare,
    .keyDestructor = dictSdsDestructor,
    .valDestructor = dictClusterNodeListDestructor};

void listCommandFree(void *command) {
    struct cmd *cmd = command;
    command_destroy(cmd);
}

/* -----------------------------------------------------------------------------
 * Key space handling
 * -------------------------------------------------------------------------- */

/* We have 16384 hash slots. The hash slot of a given key is obtained
 * as the least significant 14 bits of the crc16 of the key.
 *
 * However if the key contains the {...} pattern, only the part between
 * { and } is hashed. This may be useful in the future to force certain
 * keys to be in the same node (assuming no resharding is in progress). */
static unsigned int keyHashSlot(char *key, int keylen) {
    int s, e; /* start-end indexes of { and } */

    for (s = 0; s < keylen; s++)
        if (key[s] == '{')
            break;

    /* No '{' ? Hash the whole key. This is the base case. */
    if (s == keylen)
        return crc16(key, keylen) & 0x3FFF;

    /* '{' found? Check if we have the corresponding '}'. */
    for (e = s + 1; e < keylen; e++)
        if (key[e] == '}')
            break;

    /* No '}' or nothing between {} ? Hash the whole key. */
    if (e == keylen || e == s + 1)
        return crc16(key, keylen) & 0x3FFF;

    /* If we are here there is both a { and a } on its right. Hash
     * what is in the middle between { and }. */
    return crc16(key + s + 1, e - s - 1) & 0x3FFF;
}

static void nexcacheClusterSetError(nexcacheClusterContext *cc, int type,
                                  const char *str) {
    cc->err = type;

    assert(str != NULL);
    if (str != NULL && str != cc->errstr) {
        size_t len = strlen(str);
        len = len < (sizeof(cc->errstr) - 1) ? len : (sizeof(cc->errstr) - 1);
        memcpy(cc->errstr, str, len);
        cc->errstr[len] = '\0';
    }
}

static inline void nexcacheClusterClearError(nexcacheClusterContext *cc) {
    cc->err = 0;
    cc->errstr[0] = '\0';
}

static replyErrorType getReplyErrorType(nexcacheReply *reply) {
    assert(reply);

    if (reply->type != NEXCACHE_REPLY_ERROR)
        return CLUSTER_NO_ERROR;
    if (memcmp(reply->str, "MOVED", 5) == 0)
        return CLUSTER_ERR_MOVED;
    if (memcmp(reply->str, "ASK", 3) == 0)
        return CLUSTER_ERR_ASK;
    if (memcmp(reply->str, "TRYAGAIN", 8) == 0)
        return CLUSTER_ERR_TRYAGAIN;
    if (memcmp(reply->str, "CLUSTERDOWN", 11) == 0)
        return CLUSTER_ERR_CLUSTERDOWN;
    return CLUSTER_ERR_OTHER;
}

/* Create and initiate the cluster node structure */
static nexcacheClusterNode *createNexCacheClusterNode(void) {
    /* use calloc to guarantee all fields are zeroed */
    return vk_calloc(1, sizeof(nexcacheClusterNode));
}

/* Cleanup the cluster node structure */
static void freeNexCacheClusterNode(nexcacheClusterNode *node) {
    if (node == NULL) {
        return;
    }

    sdsfree(node->name);
    sdsfree(node->addr);
    sdsfree(node->host);
    nexcacheFree(node->con);

    if (node->acon != NULL) {
        /* Detach this cluster node from the async context. This makes sure
         * that nexcacheAsyncFree() wont attempt to update the pointer via its
         * dataCleanup and unlinkAsyncContextAndNode() */
        node->acon->data = NULL;
        nexcacheAsyncFree(node->acon);
    }
    listRelease(node->slots);
    listRelease(node->replicas);
    vk_free(node);
}

static cluster_slot *cluster_slot_create(nexcacheClusterNode *node) {
    cluster_slot *slot;

    slot = vk_calloc(1, sizeof(*slot));
    if (slot == NULL) {
        return NULL;
    }
    slot->node = node;

    if (node != NULL) {
        assert(node->role == NEXCACHE_ROLE_PRIMARY);
        if (node->slots == NULL) {
            node->slots = listCreate();
            if (node->slots == NULL) {
                cluster_slot_destroy(slot);
                return NULL;
            }

            node->slots->free = listClusterSlotDestructor;
        }

        if (listAddNodeTail(node->slots, slot) == NULL) {
            cluster_slot_destroy(slot);
            return NULL;
        }
    }

    return slot;
}

static int cluster_slot_ref_node(cluster_slot *slot, nexcacheClusterNode *node) {
    if (slot == NULL || node == NULL) {
        return NEXCACHE_ERR;
    }

    if (node->role != NEXCACHE_ROLE_PRIMARY) {
        return NEXCACHE_ERR;
    }

    if (node->slots == NULL) {
        node->slots = listCreate();
        if (node->slots == NULL) {
            return NEXCACHE_ERR;
        }

        node->slots->free = listClusterSlotDestructor;
    }

    if (listAddNodeTail(node->slots, slot) == NULL) {
        return NEXCACHE_ERR;
    }
    slot->node = node;

    return NEXCACHE_OK;
}

static void cluster_slot_destroy(cluster_slot *slot) {
    if (slot == NULL)
        return;
    slot->start = 0;
    slot->end = 0;
    slot->node = NULL;

    vk_free(slot);
}

/**
 * Handle password authentication in the synchronous API
 */
static int authenticate(nexcacheClusterContext *cc, nexcacheContext *c) {
    if (cc == NULL || c == NULL) {
        return NEXCACHE_ERR;
    }

    // Skip if no password configured
    if (cc->password == NULL) {
        return NEXCACHE_OK;
    }

    nexcacheReply *reply;
    if (cc->username != NULL) {
        reply = nexcacheCommand(c, "AUTH %s %s", cc->username, cc->password);
    } else {
        reply = nexcacheCommand(c, "AUTH %s", cc->password);
    }

    if (reply == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                              "Command AUTH reply error (NULL)");
        goto error;
    }

    if (reply->type == NEXCACHE_REPLY_ERROR) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, reply->str);
        goto error;
    }

    freeReplyObject(reply);
    return NEXCACHE_OK;

error:
    freeReplyObject(reply);

    return NEXCACHE_ERR;
}

/* Select a logical database by sending the SELECT command. */
static int select_db(nexcacheClusterContext *cc, nexcacheContext *c) {
    if (cc->select_db == 0)
        return NEXCACHE_OK;

    nexcacheReply *reply = nexcacheCommand(c, "SELECT %d", cc->select_db);
    if (reply == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Failed to select logical database");
        return NEXCACHE_ERR;
    }
    if (reply->type == NEXCACHE_REPLY_ERROR) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, reply->str);
        freeReplyObject(reply);
        return NEXCACHE_ERR;
    }
    freeReplyObject(reply);
    return NEXCACHE_OK;
}

/**
 * Return a new node with the "cluster slots" command reply.
 */
static nexcacheClusterNode *node_get_with_slots(nexcacheClusterContext *cc,
                                              char *host, int port,
                                              uint8_t role) {
    nexcacheClusterNode *node = createNexCacheClusterNode();
    if (node == NULL) {
        goto oom;
    }

    if (role == NEXCACHE_ROLE_PRIMARY) {
        node->slots = listCreate();
        if (node->slots == NULL) {
            goto oom;
        }

        node->slots->free = listClusterSlotDestructor;
    }

    node->addr = sdsnew(host);
    if (node->addr == NULL) {
        goto oom;
    }
    node->addr = sdscatfmt(node->addr, ":%i", port);
    if (node->addr == NULL) {
        goto oom;
    }
    node->host = sdsnew(host);
    if (node->host == NULL) {
        goto oom;
    }
    node->name = NULL;
    node->port = port;
    node->role = role;

    return node;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    if (node != NULL) {
        sdsfree(node->addr);
        sdsfree(node->host);
        vk_free(node);
    }
    return NULL;
}

static void cluster_nodes_swap_ctx(dict *nodes_f, dict *nodes_t) {
    dictEntry *de_f, *de_t;
    nexcacheClusterNode *node_f, *node_t;
    nexcacheContext *c;
    nexcacheAsyncContext *ac;

    if (nodes_f == NULL || nodes_t == NULL) {
        return;
    }

    dictIterator di;
    dictInitIterator(&di, nodes_t);

    while ((de_t = dictNext(&di)) != NULL) {
        node_t = dictGetVal(de_t);
        if (node_t == NULL) {
            continue;
        }

        de_f = dictFind(nodes_f, node_t->addr);
        if (de_f == NULL) {
            continue;
        }

        node_f = dictGetVal(de_f);
        if (node_f->con != NULL) {
            c = node_f->con;
            node_f->con = node_t->con;
            node_t->con = c;
        }

        if (node_f->acon != NULL) {
            ac = node_f->acon;
            node_f->acon = node_t->acon;
            node_t->acon = ac;

            node_t->acon->data = node_t;
            if (node_f->acon)
                node_f->acon->data = node_f;
        }
    }
}

/**
 * Parse the "cluster slots" command reply to nodes dict.
 */
static dict *parse_cluster_slots(nexcacheClusterContext *cc, nexcacheContext *c,
                                 nexcacheReply *reply) {
    int ret;
    cluster_slot *slot = NULL;
    dict *nodes = NULL;
    dictEntry *den;
    nexcacheReply *elem_slots;
    nexcacheReply *elem_slots_begin, *elem_slots_end;
    nexcacheReply *elem_nodes;
    nexcacheReply *elem_ip, *elem_port;
    nexcacheClusterNode *primary = NULL, *replica;
    uint32_t i, idx;

    if (reply->type != NEXCACHE_REPLY_ARRAY) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Unexpected reply type");
        goto error;
    }
    if (reply->elements == 0) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "No slot information");
        goto error;
    }

    nodes = dictCreate(&clusterNodesDictType);
    if (nodes == NULL) {
        goto oom;
    }

    for (i = 0; i < reply->elements; i++) {
        elem_slots = reply->element[i];
        if (elem_slots->type != NEXCACHE_REPLY_ARRAY ||
            elem_slots->elements < 3) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                  "Command(cluster slots) reply error: "
                                  "first sub_reply is not an array.");
            goto error;
        }

        slot = cluster_slot_create(NULL);
        if (slot == NULL) {
            goto oom;
        }

        // one slots region
        for (idx = 0; idx < elem_slots->elements; idx++) {
            if (idx == 0) {
                elem_slots_begin = elem_slots->element[idx];
                if (elem_slots_begin->type != NEXCACHE_REPLY_INTEGER) {
                    nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                          "Command(cluster slots) reply error: "
                                          "slot begin is not an integer.");
                    goto error;
                }
                slot->start = (int)(elem_slots_begin->integer);
            } else if (idx == 1) {
                elem_slots_end = elem_slots->element[idx];
                if (elem_slots_end->type != NEXCACHE_REPLY_INTEGER) {
                    nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                          "Command(cluster slots) reply error: "
                                          "slot end is not an integer.");
                    goto error;
                }

                slot->end = (int)(elem_slots_end->integer);

                if (slot->start > slot->end) {
                    nexcacheClusterSetError(
                        cc, NEXCACHE_ERR_OTHER,
                        "Command(cluster slots) reply error: "
                        "slot begin is bigger than slot end.");
                    goto error;
                }
            } else {
                elem_nodes = elem_slots->element[idx];
                if (elem_nodes->type != NEXCACHE_REPLY_ARRAY ||
                    elem_nodes->elements < 2) {
                    nexcacheClusterSetError(
                        cc, NEXCACHE_ERR_OTHER,
                        "Command(cluster slots) reply error: "
                        "nodes sub_reply is not a correct array.");
                    goto error;
                }

                elem_ip = elem_nodes->element[0];
                elem_port = elem_nodes->element[1];

                /* Validate ip element. Accept a NULL value ip (NIL type) since
                 * we will handle the unknown endpoint special. */
                if (elem_ip == NULL || (elem_ip->type != NEXCACHE_REPLY_STRING &&
                                        elem_ip->type != NEXCACHE_REPLY_NIL)) {
                    nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid node address");
                    goto error;
                }

                /* Validate port element. */
                if (elem_port == NULL || elem_port->type != NEXCACHE_REPLY_INTEGER ||
                    (elem_port->integer < 1 || elem_port->integer > UINT16_MAX)) {
                    nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid port");
                    goto error;
                }

                /* Get the received ip/host. According to the docs an unknown
                 * endpoint or an empty string can be treated as it means
                 * the same address as we sent this command to.
                 * An unknown endpoint has the type NEXCACHE_REPLY_NIL and its
                 * length is initiated to zero. */
                char *host = (elem_ip->len > 0) ? elem_ip->str : c->tcp.host;
                if (host == NULL) {
                    goto oom;
                }
                int port = elem_port->integer;

                if (idx == 2) {
                    /* Parse a primary node. */
                    sds address = sdsnew(host);
                    if (address == NULL) {
                        goto oom;
                    }
                    address = sdscatfmt(address, ":%i", port);
                    if (address == NULL) {
                        goto oom;
                    }

                    den = dictFind(nodes, address);
                    sdsfree(address);
                    if (den != NULL) {
                        /* Skip parsing this primary node since it's already known. */
                        primary = dictGetVal(den);
                        ret = cluster_slot_ref_node(slot, primary);
                        if (ret != NEXCACHE_OK) {
                            goto oom;
                        }

                        slot = NULL;
                        break;
                    }

                    primary = node_get_with_slots(cc, host, port, NEXCACHE_ROLE_PRIMARY);
                    if (primary == NULL) {
                        goto error;
                    }

                    sds key = sdsnewlen(primary->addr, sdslen(primary->addr));
                    if (key == NULL) {
                        freeNexCacheClusterNode(primary);
                        goto oom;
                    }

                    ret = dictAdd(nodes, key, primary);
                    if (ret != DICT_OK) {
                        sdsfree(key);
                        freeNexCacheClusterNode(primary);
                        goto oom;
                    }

                    ret = cluster_slot_ref_node(slot, primary);
                    if (ret != NEXCACHE_OK) {
                        goto oom;
                    }

                    slot = NULL;
                } else if (cc->flags & NEXCACHE_FLAG_PARSE_REPLICAS) {
                    replica = node_get_with_slots(cc, host, port,
                                                  NEXCACHE_ROLE_REPLICA);
                    if (replica == NULL) {
                        goto error;
                    }

                    if (primary->replicas == NULL) {
                        primary->replicas = listCreate();
                        if (primary->replicas == NULL) {
                            freeNexCacheClusterNode(replica);
                            goto oom;
                        }

                        primary->replicas->free = listClusterNodeDestructor;
                    }

                    if (listAddNodeTail(primary->replicas, replica) == NULL) {
                        freeNexCacheClusterNode(replica);
                        goto oom;
                    }
                }
            }
        }
    }

    return nodes;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    dictRelease(nodes);
    cluster_slot_destroy(slot);
    return NULL;
}

/* Keep lists of parsed replica nodes in a dict using the primary_id as key. */
static int retain_replica_node(dict *replicas, char *primary_id, nexcacheClusterNode *node) {
    sds key = sdsnew(primary_id);
    if (key == NULL)
        return NEXCACHE_ERR;

    struct hilist *replicaList;

    dictEntry *de = dictFind(replicas, key);
    if (de == NULL) {
        /* Create list to hold replicas for a primary. */
        replicaList = listCreate();
        if (replicaList == NULL) {
            sdsfree(key);
            return NEXCACHE_ERR;
        }
        replicaList->free = listClusterNodeDestructor;
        if (dictAdd(replicas, key, replicaList) != DICT_OK) {
            sdsfree(key);
            listRelease(replicaList);
            return NEXCACHE_ERR;
        }
    } else {
        sdsfree(key);
        replicaList = dictGetVal(de);
    }

    if (listAddNodeTail(replicaList, node) == NULL)
        return NEXCACHE_ERR;

    return NEXCACHE_OK;
}

/* Store parsed replica nodes in the primary nodes, which holds a list of replica
 * nodes. The `replicas` dict shall contain lists of nodes with primary_id as key. */
static int store_replica_nodes(dict *nodes, dict *replicas) {
    if (replicas == NULL)
        return NEXCACHE_OK;

    dictIterator di;
    dictInitIterator(&di, nodes);
    dictEntry *de;
    while ((de = dictNext(&di))) {
        nexcacheClusterNode *primary = dictGetVal(de);

        /* Move replica nodes related to this primary. */
        dictEntry *der = dictFind(replicas, primary->name);
        if (der != NULL) {
            assert(primary->replicas == NULL);
            /* Move replica list from replicas dict to nodes dict. */
            primary->replicas = dictGetVal(der);
            dictSetVal(replicas, der, NULL);
        }
    }
    return NEXCACHE_OK;
}

/* Parse a node from a single CLUSTER NODES line.
 * Returns NEXCACHE_OK and an allocated nexcacheClusterNode as a pointer in
 * `parsed_node`, or NEXCACHE_ERR when the parsing fails.
 * Only parse primary nodes if the `parsed_primary_id` argument is NULL,
 * otherwise replicas are also parsed and its primary_id is returned by pointer
 * via 'parsed_primary_id'.
 * The nexcacheContext used when sending the CLUSTER NODES command should be
 * provided in `c` since its destination IP address is used when no IP address
 * is found in the parsed string. */
static int parse_cluster_nodes_line(nexcacheClusterContext *cc, nexcacheContext *c, char *line,
                                    nexcacheClusterNode **parsed_node, char **parsed_primary_id) {
    char *p, *id = NULL, *addr = NULL, *flags = NULL, *primary_id = NULL,
             *link_state = NULL, *slots = NULL;
    /* Find required fields and keep a pointer to each field:
     * <id> <addr> <flags> <primary_id> <ping-sent> <pong-recv> <config-epoch> <link-state> [<slot> ...]
     */
    // clang-format off
    int i = 0;
    while ((p = strchr(line, ' ')) != NULL) {
        *p = '\0';
        switch (i++) {
            case 0: id = line; break;
            case 1: addr = line; break;
            case 2: flags = line; break;
            case 3: primary_id = line; break;
            case 7: link_state = line; break;
        }
        line = p + 1; /* Start of next field. */
        if (i == 8) { slots = line; break; }
    }
    if (i == 7 && line[0] != '\0') link_state = line;
    // clang-format on

    if (link_state == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Mandatory fields missing");
        return NEXCACHE_ERR;
    }

    /* Parse flags, a comma separated list of following flags:
     * myself, master, slave, fail?, fail, handshake, noaddr, nofailover, noflags. */
    uint8_t role = NEXCACHE_ROLE_UNKNOWN;
    while (*flags != '\0') {
        if ((p = strchr(flags, ',')) != NULL)
            *p = '\0';
        if (memcmp(flags, "master", 6) == 0)
            role = NEXCACHE_ROLE_PRIMARY;
        else if (memcmp(flags, "slave", 5) == 0)
            role = NEXCACHE_ROLE_REPLICA;
        else if (memcmp(flags, "noaddr", 6) == 0) {
            *parsed_node = NULL;
            return NEXCACHE_OK; /* Skip nodes with 'noaddr'. */
        }
        if (p == NULL) /* No more flags. */
            break;
        flags = p + 1; /* Start of next flag. */
    }
    if (role == NEXCACHE_ROLE_UNKNOWN) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Unknown role");
        return NEXCACHE_ERR;
    }

    /* Only parse replicas when requested. */
    if (role == NEXCACHE_ROLE_REPLICA && parsed_primary_id == NULL) {
        *parsed_node = NULL;
        return NEXCACHE_OK;
    }

    nexcacheClusterNode *node = createNexCacheClusterNode();
    if (node == NULL) {
        goto oom;
    }
    node->role = role;
    node->name = sdsnew(id);
    if (node->name == NULL)
        goto oom;

    /* Parse the address field: <ip:port@cport[,hostname]>
     * Remove @cport.. to get <ip>:<port> which is our dict key. */
    if ((p = strchr(addr, '@')) != NULL) {
        *p = '\0';
    }

    /* Find the required port separator. */
    if ((p = strrchr(addr, ':')) == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid node address");
        freeNexCacheClusterNode(node);
        return NEXCACHE_ERR;
    }

    /* Get the port (skip the found port separator). */
    int port = vk_atoi(p + 1, strlen(p + 1));
    if (port < 1 || port > UINT16_MAX) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid port");
        freeNexCacheClusterNode(node);
        return NEXCACHE_ERR;
    }
    node->port = port;

    /* Check that we received an ip/host address, i.e. the field
     * does not start with the port separator. */
    if (p != addr) {
        node->addr = sdsnew(addr);
        if (node->addr == NULL)
            goto oom;

        *p = '\0'; /* Cut port separator. */

        node->host = sdsnew(addr);
        if (node->host == NULL)
            goto oom;

    } else {
        /* We received an ip/host that is an empty string. According to the docs
         * we can treat this as it means the same address we sent this command to. */
        node->host = sdsnew(c->tcp.host);
        if (node->host == NULL) {
            goto oom;
        }
        /* Create a new addr field using correct host:port */
        node->addr = sdsnew(node->host);
        if (node->addr == NULL) {
            goto oom;
        }
        node->addr = sdscatfmt(node->addr, ":%i", node->port);
        if (node->addr == NULL) {
            goto oom;
        }
    }

    /* No slot parsing needed for replicas, but return primary id. */
    if (node->role == NEXCACHE_ROLE_REPLICA) {
        *parsed_primary_id = primary_id;
        *parsed_node = node;
        return NEXCACHE_OK;
    }

    node->slots = listCreate();
    if (node->slots == NULL)
        goto oom;
    node->slots->free = listClusterSlotDestructor;

    /* Parse slots when available. */
    if (slots == NULL) {
        *parsed_node = node;
        return NEXCACHE_OK;
    }
    /* Parse each slot element. */
    while (*slots != '\0') {
        if ((p = strchr(slots, ' ')) != NULL)
            *p = '\0';
        char *entry = slots;
        if (entry[0] == '[')
            break; /* Skip importing/migrating slots at string end. */

        int slot_start, slot_end;
        char *sp = strchr(entry, '-');
        if (sp == NULL) {
            slot_start = vk_atoi(entry, strlen(entry));
            slot_end = slot_start;
        } else {
            *sp = '\0';
            slot_start = vk_atoi(entry, strlen(entry));
            entry = sp + 1; // Skip '-'
            slot_end = vk_atoi(entry, strlen(entry));
        }

        /* Create a slot entry owned by the node. */
        cluster_slot *slot = cluster_slot_create(node);
        if (slot == NULL)
            goto oom;
        slot->start = (uint32_t)slot_start;
        slot->end = (uint32_t)slot_end;

        if (p == NULL) /* Check if this was the last entry. */
            break;
        slots = p + 1; /* Start of next entry. */
    }
    *parsed_node = node;
    return NEXCACHE_OK;

oom:
    freeNexCacheClusterNode(node);
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    return NEXCACHE_ERR;
}

/**
 * Parse the "cluster nodes" command reply to nodes dict.
 */
static dict *parse_cluster_nodes(nexcacheClusterContext *cc, nexcacheContext *c, nexcacheReply *reply) {
    dict *nodes = NULL;
    int slot_ranges_found = 0;
    int add_replicas = cc->flags & NEXCACHE_FLAG_PARSE_REPLICAS;
    dict *replicas = NULL;

    if (reply->type != NEXCACHE_REPLY_STRING && reply->type != NEXCACHE_REPLY_VERB) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Unexpected reply type");
        goto error;
    }

    nodes = dictCreate(&clusterNodesDictType);
    if (nodes == NULL) {
        goto oom;
    }

    char *lines = reply->str; /* NULL terminated string. */
    char *p, *line;
    while ((p = strchr(lines, '\n')) != NULL) {
        *p = '\0';
        line = lines;
        lines = p + 1; /* Start of next line. */

        char *primary_id;
        nexcacheClusterNode *node;
        if (parse_cluster_nodes_line(cc, c, line, &node, add_replicas ? &primary_id : NULL) != NEXCACHE_OK)
            goto error;
        if (node == NULL)
            continue; /* Line skipped. */
        if (node->role == NEXCACHE_ROLE_PRIMARY) {
            sds key = sdsnew(node->addr);
            if (key == NULL) {
                freeNexCacheClusterNode(node);
                goto oom;
            }
            if (dictFind(nodes, key) != NULL) {
                nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                      "Duplicate addresses in cluster nodes response");
                sdsfree(key);
                freeNexCacheClusterNode(node);
                goto error;
            }
            if (dictAdd(nodes, key, node) != DICT_OK) {
                sdsfree(key);
                freeNexCacheClusterNode(node);
                goto oom;
            }
            slot_ranges_found += listLength(node->slots);

        } else {
            assert(node->role == NEXCACHE_ROLE_REPLICA);
            if (replicas == NULL) {
                if ((replicas = dictCreate(&clusterNodeListDictType)) == NULL) {
                    freeNexCacheClusterNode(node);
                    goto oom;
                }
            }
            /* Retain parsed replica nodes until all primaries are parsed. */
            if (retain_replica_node(replicas, primary_id, node) != NEXCACHE_OK) {
                freeNexCacheClusterNode(node);
                goto oom;
            }
        }
    }

    if (slot_ranges_found == 0) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "No slot information");
        goto error;
    }

    /* Store the retained replica nodes in primary nodes. */
    if (store_replica_nodes(nodes, replicas) != NEXCACHE_OK) {
        goto oom;
    }
    dictRelease(replicas);

    return nodes;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    dictRelease(replicas);
    dictRelease(nodes);
    return NULL;
}

/* Sends CLUSTER SLOTS or CLUSTER NODES to the node with context c. */
static int clusterUpdateRouteSendCommand(nexcacheClusterContext *cc,
                                         nexcacheContext *c) {
    const char *cmd = (cc->flags & NEXCACHE_FLAG_USE_CLUSTER_NODES ?
                           NEXCACHE_COMMAND_CLUSTER_NODES :
                           NEXCACHE_COMMAND_CLUSTER_SLOTS);
    if (nexcacheAppendCommand(c, cmd) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }
    /* Flush buffer to socket. */
    if (nexcacheBufferWrite(c, NULL) == NEXCACHE_ERR) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

/* Receives and handles a CLUSTER SLOTS or CLUSTER NODES reply from node with
 * context c. */
static int clusterUpdateRouteHandleReply(nexcacheClusterContext *cc,
                                         nexcacheContext *c) {
    nexcacheReply *reply = NULL;
    if (nexcacheGetReply(c, (void **)&reply) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }
    if (reply->type == NEXCACHE_REPLY_ERROR) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, reply->str);
        freeReplyObject(reply);
        return NEXCACHE_ERR;
    }

    dict *nodes;
    if (cc->flags & NEXCACHE_FLAG_USE_CLUSTER_NODES) {
        nodes = parse_cluster_nodes(cc, c, reply);
    } else {
        nodes = parse_cluster_slots(cc, c, reply);
    }
    freeReplyObject(reply);
    return updateNodesAndSlotmap(cc, nodes);
}

/* Update known cluster nodes with a new collection of nexcacheClusterNodes.
 * Will also update the slot-to-node lookup table for the new nodes. */
static int updateNodesAndSlotmap(nexcacheClusterContext *cc, dict *nodes) {
    if (nodes == NULL) {
        return NEXCACHE_ERR;
    }

    /* Create a slot to nexcacheClusterNode lookup table */
    nexcacheClusterNode **table;
    table = vk_calloc(VALKEYCLUSTER_SLOTS, sizeof(nexcacheClusterNode *));
    if (table == NULL) {
        goto oom;
    }

    dictIterator di;
    dictInitIterator(&di, nodes);

    dictEntry *de;
    while ((de = dictNext(&di))) {
        nexcacheClusterNode *node = dictGetVal(de);
        if (node->role != NEXCACHE_ROLE_PRIMARY) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                  "Node role must be primary");
            goto error;
        }

        if (node->slots == NULL) {
            continue;
        }

        listIter li;
        listRewind(node->slots, &li);

        listNode *ln;
        while ((ln = listNext(&li))) {
            cluster_slot *slot = listNodeValue(ln);
            if (slot->start > slot->end || slot->end >= VALKEYCLUSTER_SLOTS) {
                nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                      "Slot region for node is invalid");
                goto error;
            }
            for (uint32_t i = slot->start; i <= slot->end; i++) {
                if (table[i] != NULL) {
                    nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                          "Different node holds same slot");
                    goto error;
                }
                table[i] = node;
            }
        }
    }

    /* Update slot-to-node table before changing cc->nodes since
     * removal of nodes might trigger user callbacks which may
     * send commands, which depend on the slot-to-node table. */
    if (cc->table != NULL) {
        vk_free(cc->table);
    }
    cc->table = table;

    cc->route_version++;

    // Move all libnexcache contexts in cc->nodes to nodes
    cluster_nodes_swap_ctx(cc->nodes, nodes);

    /* Replace cc->nodes before releasing the old dict since
     * the release procedure might access cc->nodes. */
    dict *oldnodes = cc->nodes;
    cc->nodes = nodes;
    dictRelease(oldnodes);

    if (cc->event_callback != NULL) {
        cc->event_callback(cc, VALKEYCLUSTER_EVENT_SLOTMAP_UPDATED,
                           cc->event_privdata);
        if (cc->route_version == 1) {
            /* Special event the first time the slotmap was updated. */
            cc->event_callback(cc, VALKEYCLUSTER_EVENT_READY,
                               cc->event_privdata);
        }
    }
    cc->need_update_route = 0;
    return NEXCACHE_OK;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough
error:
    vk_free(table);
    dictRelease(nodes);
    return NEXCACHE_ERR;
}

int nexcacheClusterUpdateSlotmap(nexcacheClusterContext *cc) {
    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    nexcacheClusterNode *node;
    dictEntry *de;

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    while ((de = dictNext(&di)) != NULL) {
        node = dictGetVal(de);

        /* Use existing connection or (re)connect to the node. */
        nexcacheContext *c = nexcacheClusterGetNexCacheContext(cc, node);
        if (c == NULL)
            continue;
        if (clusterUpdateRouteSendCommand(cc, c) != NEXCACHE_OK ||
            clusterUpdateRouteHandleReply(cc, c) != NEXCACHE_OK) {
            nexcacheFree(node->con);
            node->con = NULL;
            continue;
        }
        nexcacheClusterClearError(cc);
        return NEXCACHE_OK;
    }

    return NEXCACHE_ERR;
}

static int nexcacheClusterContextInit(nexcacheClusterContext *cc,
                                    const nexcacheClusterOptions *options) {
    cc->nodes = dictCreate(&clusterNodesDictType);
    if (cc->nodes == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }
    cc->requests = listCreate();
    if (cc->requests == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }
    cc->requests->free = listCommandFree;

    int supported_options = (NEXCACHE_OPT_USE_CLUSTER_NODES | NEXCACHE_OPT_USE_REPLICAS |
                             NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE | NEXCACHE_OPT_REUSEADDR |
                             NEXCACHE_OPT_PREFER_IPV4 | NEXCACHE_OPT_PREFER_IPV6 |
                             NEXCACHE_OPT_PREFER_IP_UNSPEC | NEXCACHE_OPT_MPTCP);
    if (options->options & ~supported_options) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Unsupported options");
        return NEXCACHE_ERR;
    }
    cc->options = options->options;

    if (options->options & NEXCACHE_OPT_USE_CLUSTER_NODES) {
        cc->flags |= NEXCACHE_FLAG_USE_CLUSTER_NODES;
    }
    if (options->options & NEXCACHE_OPT_USE_REPLICAS) {
        cc->flags |= NEXCACHE_FLAG_PARSE_REPLICAS;
    }
    if (options->options & NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE) {
        cc->flags |= NEXCACHE_FLAG_BLOCKING_INITIAL_UPDATE;
    }
    if (options->max_retry > 0) {
        cc->max_retry_count = options->max_retry;
    } else {
        cc->max_retry_count = CLUSTER_DEFAULT_MAX_RETRY_COUNT;
    }
    if (options->select_db > 0) {
        cc->select_db = options->select_db;
    }
    if (options->initial_nodes != NULL &&
        nexcacheClusterSetOptionAddNodes(cc, options->initial_nodes) != NEXCACHE_OK) {
        return NEXCACHE_ERR; /* err and errstr already set. */
    }
    if (options->connect_timeout != NULL &&
        nexcacheClusterSetOptionConnectTimeout(cc, *options->connect_timeout) != NEXCACHE_OK) {
        return NEXCACHE_ERR; /* err and errstr already set. */
    }
    if (options->command_timeout != NULL &&
        nexcacheClusterSetOptionTimeout(cc, *options->command_timeout) != NEXCACHE_OK) {
        return NEXCACHE_ERR; /* err and errstr already set. */
    }
    if (options->username != NULL &&
        nexcacheClusterSetOptionUsername(cc, options->username) != NEXCACHE_OK) {
        return NEXCACHE_ERR; /* err and errstr already set. */
    }
    if (options->password != NULL &&
        nexcacheClusterSetOptionPassword(cc, options->password) != NEXCACHE_OK) {
        return NEXCACHE_ERR; /* err and errstr already set. */
    }
    if (options->connect_callback) {
        cc->on_connect = options->connect_callback;
    }
    if (options->event_callback) {
        cc->event_callback = options->event_callback;
        cc->event_privdata = options->event_privdata;
    }
    if (options->tls) {
        cc->tls = options->tls;
        cc->tls_init_fn = options->tls_init_fn;
    }

    return NEXCACHE_OK;
}

void nexcacheClusterFree(nexcacheClusterContext *cc) {
    if (cc == NULL)
        return;

    if (cc->event_callback) {
        cc->event_callback(cc, VALKEYCLUSTER_EVENT_FREE_CONTEXT,
                           cc->event_privdata);
    }

    vk_free(cc->connect_timeout);
    vk_free(cc->command_timeout);
    vk_free(cc->username);
    vk_free(cc->password);
    vk_free(cc->table);
    dictRelease(cc->nodes);
    listRelease(cc->requests);

    memset(cc, 0xff, sizeof(*cc));
    vk_free(cc);
}

nexcacheClusterContext *nexcacheClusterConnectWithOptions(const nexcacheClusterOptions *options) {
    nexcacheClusterContext *cc;

    cc = vk_calloc(1, sizeof(nexcacheClusterContext));
    if (cc == NULL)
        return NULL;

    if (nexcacheClusterContextInit(cc, options) == NEXCACHE_OK) {
        /* Only connect if options are ok. */
        nexcacheClusterUpdateSlotmap(cc);
    }
    return cc;
}

nexcacheClusterContext *nexcacheClusterConnect(const char *addrs) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = addrs;

    return nexcacheClusterConnectWithOptions(&options);
}

nexcacheClusterContext *nexcacheClusterConnectWithTimeout(const char *addrs,
                                                      const struct timeval tv) {
    nexcacheClusterOptions options = {0};
    options.initial_nodes = addrs;
    options.connect_timeout = &tv;

    return nexcacheClusterConnectWithOptions(&options);
}

static int nexcacheClusterSetOptionAddNode(nexcacheClusterContext *cc, const char *addr) {
    dictEntry *node_entry;
    nexcacheClusterNode *node = NULL;
    int port, ret;
    sds ip = NULL;

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    sds addr_sds = sdsnew(addr);
    if (addr_sds == NULL) {
        goto oom;
    }
    node_entry = dictFind(cc->nodes, addr_sds);
    sdsfree(addr_sds);
    if (node_entry == NULL) {
        char *p;

        /* Find the last occurrence of the port separator since
         * IPv6 addresses can contain ':' */
        if ((p = strrchr(addr, ':')) == NULL) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                  "server address is incorrect, port separator missing.");
            return NEXCACHE_ERR;
        }
        // p includes separator

        if (p - addr <= 0) { /* length until separator */
            nexcacheClusterSetError(
                cc, NEXCACHE_ERR_OTHER,
                "server address is incorrect, address part missing.");
            return NEXCACHE_ERR;
        }

        ip = sdsnewlen(addr, p - addr);
        if (ip == NULL) {
            goto oom;
        }
        p++; // remove separator character

        if (strlen(p) <= 0) {
            nexcacheClusterSetError(
                cc, NEXCACHE_ERR_OTHER,
                "server address is incorrect, port part missing.");
            goto error;
        }

        port = vk_atoi(p, strlen(p));
        if (port <= 0) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                  "server port is incorrect");
            goto error;
        }

        node = createNexCacheClusterNode();
        if (node == NULL) {
            goto oom;
        }

        node->addr = sdsnew(addr);
        if (node->addr == NULL) {
            goto oom;
        }

        node->host = ip;
        node->port = port;

        sds key = sdsnewlen(node->addr, sdslen(node->addr));
        if (key == NULL) {
            goto oom;
        }
        ret = dictAdd(cc->nodes, key, node);
        if (ret != DICT_OK) {
            sdsfree(key);
            goto oom;
        }
    }

    return NEXCACHE_OK;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    sdsfree(ip);
    if (node != NULL) {
        sdsfree(node->addr);
        vk_free(node);
    }
    return NEXCACHE_ERR;
}

static int nexcacheClusterSetOptionAddNodes(nexcacheClusterContext *cc,
                                          const char *addrs) {
    int ret;
    sds *address = NULL;
    int address_count = 0;
    int i;

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    /* Split into individual addresses. */
    address = sdssplitlen(addrs, strlen(addrs), ",", strlen(","), &address_count);
    if (address == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    if (address_count <= 0) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                              "invalid server addresses (example format: "
                              "127.0.0.1:1234,127.0.0.2:5678)");
        sdsfreesplitres(address, address_count);
        return NEXCACHE_ERR;
    }

    for (i = 0; i < address_count; i++) {
        ret = nexcacheClusterSetOptionAddNode(cc, address[i]);
        if (ret != NEXCACHE_OK) {
            sdsfreesplitres(address, address_count);
            return NEXCACHE_ERR;
        }
    }

    sdsfreesplitres(address, address_count);

    return NEXCACHE_OK;
}

/**
 * Configure a username used during authentication, see
 * the NexCache AUTH command.
 * Disabled by default. Can be disabled again by providing an
 * empty string or a null pointer.
 */
static int nexcacheClusterSetOptionUsername(nexcacheClusterContext *cc,
                                          const char *username) {
    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    // Disabling option
    if (username == NULL || username[0] == '\0') {
        vk_free(cc->username);
        cc->username = NULL;
        return NEXCACHE_OK;
    }

    vk_free(cc->username);
    cc->username = vk_strdup(username);
    if (cc->username == NULL) {
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

/**
 * Configure a password used when connecting to password-protected
 * NexCache instances. (See NexCache AUTH command)
 */
static int nexcacheClusterSetOptionPassword(nexcacheClusterContext *cc,
                                          const char *password) {

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    // Disabling use of password
    if (password == NULL || password[0] == '\0') {
        vk_free(cc->password);
        cc->password = NULL;
        return NEXCACHE_OK;
    }

    vk_free(cc->password);
    cc->password = vk_strdup(password);
    if (cc->password == NULL) {
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

static int nexcacheClusterSetOptionConnectTimeout(nexcacheClusterContext *cc,
                                                const struct timeval tv) {

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    if (cc->connect_timeout == NULL) {
        cc->connect_timeout = vk_malloc(sizeof(struct timeval));
        if (cc->connect_timeout == NULL) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
            return NEXCACHE_ERR;
        }
    }

    memcpy(cc->connect_timeout, &tv, sizeof(struct timeval));

    return NEXCACHE_OK;
}

int nexcacheClusterSetOptionTimeout(nexcacheClusterContext *cc,
                                  const struct timeval tv) {
    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    if (cc->command_timeout == NULL ||
        cc->command_timeout->tv_sec != tv.tv_sec ||
        cc->command_timeout->tv_usec != tv.tv_usec) {

        if (cc->command_timeout == NULL) {
            cc->command_timeout = vk_malloc(sizeof(struct timeval));
            if (cc->command_timeout == NULL) {
                nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
                return NEXCACHE_ERR;
            }
        }

        memcpy(cc->command_timeout, &tv, sizeof(struct timeval));

        /* Set timeout on already connected nodes */
        if (dictSize(cc->nodes) > 0) {
            dictEntry *de;
            nexcacheClusterNode *node;

            dictIterator di;
            dictInitIterator(&di, cc->nodes);

            while ((de = dictNext(&di)) != NULL) {
                node = dictGetVal(de);
                if (node->acon) {
                    nexcacheAsyncSetTimeout(node->acon, tv);
                }
                if (node->con && node->con->err == 0) {
                    nexcacheSetTimeout(node->con, tv);
                }

                if (node->replicas && listLength(node->replicas) > 0) {
                    nexcacheClusterNode *replica;
                    listNode *ln;

                    listIter li;
                    listRewind(node->replicas, &li);

                    while ((ln = listNext(&li)) != NULL) {
                        replica = listNodeValue(ln);
                        if (replica->acon) {
                            nexcacheAsyncSetTimeout(replica->acon, tv);
                        }
                        if (replica->con && replica->con->err == 0) {
                            nexcacheSetTimeout(replica->con, tv);
                        }
                    }
                }
            }
        }
    }

    return NEXCACHE_OK;
}

nexcacheContext *nexcacheClusterGetNexCacheContext(nexcacheClusterContext *cc,
                                             nexcacheClusterNode *node) {
    nexcacheContext *c = NULL;
    if (node == NULL) {
        return NULL;
    }

    c = node->con;
    if (c != NULL) {
        if (c->err) {
            nexcacheReconnect(c);

            if (cc->on_connect) {
                cc->on_connect(c, c->err ? NEXCACHE_ERR : NEXCACHE_OK);
            }

            if (cc->tls && cc->tls_init_fn(c, cc->tls) != NEXCACHE_OK) {
                nexcacheClusterSetError(cc, c->err, c->errstr);
            }
            /* Authenticate and select a logical database when configured.
             * cc->err and cc->errstr are set when failing. */
            authenticate(cc, c);
            select_db(cc, c);
        }

        return c;
    }

    if (node->host == NULL || node->port <= 0) {
        return NULL;
    }

    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, node->host, node->port);
    options.connect_timeout = cc->connect_timeout;
    options.command_timeout = cc->command_timeout;
    options.options = cc->options;

    c = nexcacheConnectWithOptions(&options);
    if (c == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NULL;
    }

    if (cc->on_connect) {
        cc->on_connect(c, c->err ? NEXCACHE_ERR : NEXCACHE_OK);
    }

    if (c->err) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        nexcacheFree(c);
        return NULL;
    }

    if (cc->tls && cc->tls_init_fn(c, cc->tls) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        nexcacheFree(c);
        return NULL;
    }

    if (authenticate(cc, c) != NEXCACHE_OK) {
        nexcacheFree(c);
        return NULL;
    }
    if (select_db(cc, c) != NEXCACHE_OK) {
        nexcacheFree(c);
        return NULL;
    }

    node->con = c;

    return c;
}

static nexcacheClusterNode *node_get_by_table(nexcacheClusterContext *cc,
                                            uint32_t slot_num) {
    if (cc == NULL) {
        return NULL;
    }

    if (slot_num >= VALKEYCLUSTER_SLOTS) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "invalid slot");
        return NULL;
    }

    if (cc->table == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "slotmap not available");
        return NULL;
    }

    if (cc->table[slot_num] == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                              "slot not served by any node");
        return NULL;
    }

    return cc->table[slot_num];
}

/* Helper function for the nexcacheClusterAppendCommand* family of functions.
 *
 * Write a formatted command to the output buffer. When this family
 * is used, you need to call nexcacheGetReply yourself to retrieve
 * the reply (or replies in pub/sub).
 */
static int nexcacheClusterAppendCommandInternal(nexcacheClusterContext *cc,
                                              struct cmd *command) {

    nexcacheClusterNode *node;
    nexcacheContext *c = NULL;

    if (cc == NULL || command == NULL) {
        return NEXCACHE_ERR;
    }

    node = node_get_by_table(cc, (uint32_t)command->slot_num);
    if (node == NULL) {
        return NEXCACHE_ERR;
    }

    c = nexcacheClusterGetNexCacheContext(cc, node);
    if (c == NULL) {
        return NEXCACHE_ERR;
    } else if (c->err) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    if (nexcacheAppendFormattedCommand(c, command->cmd, command->clen) !=
        NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

/* Helper functions for the nexcacheClusterGetReply* family of functions.
 */
static int nexcacheClusterGetReplyFromNode(nexcacheClusterContext *cc,
                                         nexcacheClusterNode *node,
                                         void **reply) {
    nexcacheContext *c;

    if (cc == NULL || node == NULL || reply == NULL)
        return NEXCACHE_ERR;

    c = node->con;
    if (c == NULL) {
        return NEXCACHE_ERR;
    } else if (c->err) {
        if (cc->need_update_route == 0) {
            cc->retry_count++;
            if (cc->retry_count > cc->max_retry_count) {
                cc->need_update_route = 1;
                cc->retry_count = 0;
            }
        }
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    if (nexcacheGetReply(c, reply) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    if (getReplyErrorType(*reply) == CLUSTER_ERR_MOVED)
        cc->need_update_route = 1;

    return NEXCACHE_OK;
}

/* Parses a MOVED or ASK error reply and returns the destination node. The slot
 * is returned by pointer, if provided. When the parsed endpoint/IP is an empty
 * string the address from which the reply was sent from is used instead, as
 * described in the NexCache Cluster Specification. This address is provided via
 * the nexcacheContext given in 'c'. */
static nexcacheClusterNode *getNodeFromRedirectReply(nexcacheClusterContext *cc,
                                                   nexcacheContext *c,
                                                   nexcacheReply *reply,
                                                   int *slotptr) {
    nexcacheClusterNode *node = NULL;
    sds key = NULL;
    sds endpoint = NULL;
    char *str = reply->str;

    /* Expecting ["ASK" | "MOVED", "<slot>", "<endpoint>:<port>"] */
    char *p, *slot = NULL, *addr = NULL;
    int field = 0;
    while (*str != '\0') {
        // clang-format off
        switch (field++) {
            // Skip field 0, i.e. ASK/MOVED
            case 1: slot = str; break;
            case 2: addr = str; break;
        }
        // clang-format on
        if ((p = strchr(str, ' ')) == NULL)
            break; /* No more fields. */
        *p = '\0';
        str = p + 1; /* Start of next field. */
    }

    /* Make sure all expected fields are found. */
    if (addr == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Failed to parse redirect");
        return NULL;
    }
    /* Find the last occurrence of the port separator since
     * IPv6 addresses can contain ':' */
    if ((p = strrchr(addr, ':')) == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid address in redirect");
        return NULL;
    }
    /* Get the port (skip the found port separator). */
    int port = vk_atoi(p + 1, strlen(p + 1));
    if (port < 1 || port > UINT16_MAX) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid port in redirect");
        return NULL;
    }

    /* Check that we received an ip/host address, i.e. the field
     * does not start with the port separator. */
    if (p != addr) {
        key = sdsnew(addr);
        if (key == NULL) {
            goto oom;
        }
        *p = '\0'; /* Cut port separator and port. */

        endpoint = sdsnew(addr);
        if (endpoint == NULL) {
            goto oom;
        }
    } else {
        /* We received an ip/host that is an empty string. According to the docs
         * we can treat this as it means the same address we received the reply from. */
        endpoint = sdsnew(c->tcp.host);
        if (endpoint == NULL) {
            goto oom;
        }

        key = sdsdup(endpoint);
        if (key == NULL) {
            goto oom;
        }
        key = sdscatfmt(key, ":%i", port);
        if (key == NULL) {
            goto oom;
        }
    }

    /* Parse slot if requested. */
    if (slotptr != NULL) {
        *slotptr = vk_atoi(slot, strlen(slot));
    }

    /* Get the node if already known. */
    dictEntry *de = dictFind(cc->nodes, key);
    if (de != NULL) {
        sdsfree(key);
        sdsfree(endpoint);
        return dictGetVal(de);
    }

    /* Add this node since it was unknown */
    node = createNexCacheClusterNode();
    if (node == NULL) {
        goto oom;
    }
    node->role = NEXCACHE_ROLE_PRIMARY;
    node->host = endpoint;
    node->port = port;
    node->addr = sdsdup(key);
    if (node->addr == NULL) {
        goto oom;
    }

    if (dictAdd(cc->nodes, key, node) != DICT_OK) {
        goto oom;
    }
    return node;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    sdsfree(key);
    sdsfree(endpoint);
    if (node != NULL) {
        sdsfree(node->addr);
        vk_free(node);
    }
    return NULL;
}

static void *nexcache_cluster_command_execute(nexcacheClusterContext *cc,
                                            struct cmd *command) {
    void *reply = NULL;
    nexcacheClusterNode *node;
    nexcacheContext *c = NULL;
    nexcacheContext *c_updating_route = NULL;

retry:

    node = node_get_by_table(cc, (uint32_t)command->slot_num);
    if (node == NULL) {
        /* Update the slotmap since the slot is not served. */
        if (nexcacheClusterUpdateSlotmap(cc) != NEXCACHE_OK) {
            goto error;
        }
        node = node_get_by_table(cc, (uint32_t)command->slot_num);
        if (node == NULL) {
            /* Return error since the slot is still not served. */
            goto error;
        }
    }

    c = nexcacheClusterGetNexCacheContext(cc, node);
    if (c == NULL || c->err) {
        /* Failed to connect. Maybe there was a failover and this node is gone.
         * Update slotmap to find out. */
        if (nexcacheClusterUpdateSlotmap(cc) != NEXCACHE_OK) {
            goto error;
        }

        node = node_get_by_table(cc, (uint32_t)command->slot_num);
        if (node == NULL) {
            goto error;
        }
        c = nexcacheClusterGetNexCacheContext(cc, node);
        if (c == NULL) {
            goto error;
        } else if (c->err) {
            nexcacheClusterSetError(cc, c->err, c->errstr);
            goto error;
        }
    }

moved_retry:
ask_retry:

    if (nexcacheAppendFormattedCommand(c, command->cmd, command->clen) !=
        NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        goto error;
    }

    /* If update slotmap has been scheduled, do that in the same pipeline. */
    if (cc->need_update_route && c_updating_route == NULL) {
        if (clusterUpdateRouteSendCommand(cc, c) == NEXCACHE_OK) {
            c_updating_route = c;
        }
    }

    if (nexcacheGetReply(c, &reply) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        /* We may need to update the slotmap if this node is removed from the
         * cluster, but the current request may have already timed out so we
         * schedule it for later. */
        if (c->err != NEXCACHE_ERR_OOM)
            cc->need_update_route = 1;
        goto error;
    }

    replyErrorType error_type = getReplyErrorType(reply);
    if (error_type > CLUSTER_NO_ERROR && error_type < CLUSTER_ERR_OTHER) {
        cc->retry_count++;
        if (cc->retry_count > cc->max_retry_count) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_CLUSTER_TOO_MANY_RETRIES,
                                  "too many cluster retries");
            goto error;
        }

        int slot = -1;
        switch (error_type) {
        case CLUSTER_ERR_MOVED:
            node = getNodeFromRedirectReply(cc, c, reply, &slot);
            freeReplyObject(reply);
            reply = NULL;

            if (node == NULL) {
                /* Failed to parse redirect. Specific error already set. */
                goto error;
            }

            /* Update the slot mapping entry for this slot. */
            if (slot >= 0) {
                cc->table[slot] = node;
            }

            if (c_updating_route == NULL) {
                if (clusterUpdateRouteSendCommand(cc, c) == NEXCACHE_OK) {
                    /* Deferred update route using the node that sent the
                     * redirect. */
                    c_updating_route = c;
                } else if (nexcacheClusterUpdateSlotmap(cc) == NEXCACHE_OK) {
                    /* Synchronous update route successful using new connection. */
                    nexcacheClusterClearError(cc);
                } else {
                    /* Failed to update route. Specific error already set. */
                    goto error;
                }
            }

            c = nexcacheClusterGetNexCacheContext(cc, node);
            if (c == NULL) {
                goto error;
            } else if (c->err) {
                nexcacheClusterSetError(cc, c->err, c->errstr);
                goto error;
            }

            goto moved_retry;
        case CLUSTER_ERR_ASK:
            node = getNodeFromRedirectReply(cc, c, reply, NULL);
            if (node == NULL) {
                goto error;
            }

            freeReplyObject(reply);
            reply = NULL;

            c = nexcacheClusterGetNexCacheContext(cc, node);
            if (c == NULL) {
                goto error;
            } else if (c->err) {
                nexcacheClusterSetError(cc, c->err, c->errstr);
                goto error;
            }

            reply = nexcacheCommand(c, NEXCACHE_COMMAND_ASKING);
            if (reply == NULL) {
                nexcacheClusterSetError(cc, c->err, c->errstr);
                goto error;
            }

            freeReplyObject(reply);
            reply = NULL;

            goto ask_retry;
        case CLUSTER_ERR_TRYAGAIN:
        case CLUSTER_ERR_CLUSTERDOWN:
            freeReplyObject(reply);
            reply = NULL;
            goto retry;

        default:

            break;
        }
    }

    goto done;

error:
    if (reply) {
        freeReplyObject(reply);
        reply = NULL;
    }

done:
    if (c_updating_route) {
        /* Deferred CLUSTER SLOTS or CLUSTER NODES in progress. Wait for the
         * reply and handle it. */
        if (clusterUpdateRouteHandleReply(cc, c_updating_route) != NEXCACHE_OK) {
            /* Clear error and update synchronously using another node. */
            nexcacheClusterClearError(cc);
            if (nexcacheClusterUpdateSlotmap(cc) != NEXCACHE_OK) {
                /* Clear the reply to indicate failure. */
                freeReplyObject(reply);
                reply = NULL;
            }
        }
    }

    return reply;
}

/* Prepare command by parsing the string to find the key and to get the slot. */
static int prepareCommand(nexcacheClusterContext *cc, struct cmd *command) {
    if (command->cmd == NULL || command->clen <= 0) {
        return NEXCACHE_ERR;
    }

    nexcache_parse_cmd(command);
    if (command->result == CMD_PARSE_ENOMEM) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }
    if (command->result != CMD_PARSE_OK) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_PROTOCOL, command->errstr);
        return NEXCACHE_ERR;
    }
    if (command->key.len == 0) {
        nexcacheClusterSetError(
            cc, NEXCACHE_ERR_OTHER,
            "No keys in command(must have keys for nexcache cluster mode)");
        return NEXCACHE_ERR;
    }
    command->slot_num = keyHashSlot(command->key.start, command->key.len);
    return NEXCACHE_OK;
}

void *nexcacheClusterFormattedCommand(nexcacheClusterContext *cc, char *cmd,
                                    int len) {
    nexcacheReply *reply = NULL;
    struct cmd *command = NULL;

    if (cc == NULL) {
        return NULL;
    }

    nexcacheClusterClearError(cc);

    command = command_get();
    if (command == NULL) {
        goto oom;
    }
    command->cmd = cmd;
    command->clen = len;

    if (prepareCommand(cc, command) != NEXCACHE_OK) {
        goto error;
    }

    reply = nexcache_cluster_command_execute(cc, command);
    command->cmd = NULL;
    command_destroy(command);
    cc->retry_count = 0;
    return reply;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    if (command != NULL) {
        command->cmd = NULL;
        command_destroy(command);
    }
    cc->retry_count = 0;
    return NULL;
}

void *nexcacheClustervCommand(nexcacheClusterContext *cc, const char *format,
                            va_list ap) {
    nexcacheReply *reply;
    char *cmd;
    int len;

    if (cc == NULL) {
        return NULL;
    }

    len = nexcachevFormatCommand(&cmd, format, ap);

    if (len == -1) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NULL;
    } else if (len == -2) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid format string");
        return NULL;
    }

    reply = nexcacheClusterFormattedCommand(cc, cmd, len);

    vk_free(cmd);

    return reply;
}

void *nexcacheClusterCommand(nexcacheClusterContext *cc, const char *format, ...) {
    va_list ap;
    nexcacheReply *reply = NULL;

    va_start(ap, format);
    reply = nexcacheClustervCommand(cc, format, ap);
    va_end(ap);

    return reply;
}

void *nexcacheClustervCommandToNode(nexcacheClusterContext *cc,
                                  nexcacheClusterNode *node, const char *format,
                                  va_list ap) {
    nexcacheContext *c;
    int ret;
    void *reply;
    int updating_slotmap = 0;

    c = nexcacheClusterGetNexCacheContext(cc, node);
    if (c == NULL) {
        return NULL;
    } else if (c->err) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NULL;
    }

    nexcacheClusterClearError(cc);

    ret = nexcachevAppendCommand(c, format, ap);

    if (ret != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NULL;
    }

    if (cc->need_update_route) {
        /* Pipeline slotmap update on the same connection. */
        if (clusterUpdateRouteSendCommand(cc, c) == NEXCACHE_OK) {
            updating_slotmap = 1;
        }
    }

    if (nexcacheGetReply(c, &reply) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        if (c->err != NEXCACHE_ERR_OOM)
            cc->need_update_route = 1;
        return NULL;
    }

    if (updating_slotmap) {
        /* Handle reply from pipelined CLUSTER SLOTS or CLUSTER NODES. */
        if (clusterUpdateRouteHandleReply(cc, c) != NEXCACHE_OK) {
            /* Ignore error. Update will be triggered on the next command. */
            nexcacheClusterClearError(cc);
        }
    }

    return reply;
}

void *nexcacheClusterCommandToNode(nexcacheClusterContext *cc,
                                 nexcacheClusterNode *node, const char *format,
                                 ...) {
    va_list ap;
    nexcacheReply *reply = NULL;

    va_start(ap, format);
    reply = nexcacheClustervCommandToNode(cc, node, format, ap);
    va_end(ap);

    return reply;
}

void *nexcacheClusterCommandArgv(nexcacheClusterContext *cc, int argc,
                               const char **argv, const size_t *argvlen) {
    nexcacheReply *reply = NULL;
    char *cmd;
    int len;

    len = nexcacheFormatCommandArgv(&cmd, argc, argv, argvlen);
    if (len == -1) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NULL;
    }

    reply = nexcacheClusterFormattedCommand(cc, cmd, len);

    vk_free(cmd);

    return reply;
}

int nexcacheClusterAppendFormattedCommand(nexcacheClusterContext *cc, char *cmd,
                                        int len) {
    struct cmd *command = NULL;

    command = command_get();
    if (command == NULL) {
        goto oom;
    }
    command->cmd = cmd;
    command->clen = len;

    if (prepareCommand(cc, command) != NEXCACHE_OK) {
        goto error;
    }

    if (nexcacheClusterAppendCommandInternal(cc, command) != NEXCACHE_OK) {
        goto error;
    }

    command->cmd = NULL;

    if (listAddNodeTail(cc->requests, command) == NULL) {
        goto oom;
    }
    return NEXCACHE_OK;

oom:
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    if (command != NULL) {
        command->cmd = NULL;
        command_destroy(command);
    }
    return NEXCACHE_ERR;
}

int nexcacheClustervAppendCommand(nexcacheClusterContext *cc, const char *format,
                                va_list ap) {
    int ret;
    char *cmd;
    int len;

    len = nexcachevFormatCommand(&cmd, format, ap);
    if (len == -1) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    } else if (len == -2) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid format string");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAppendFormattedCommand(cc, cmd, len);

    vk_free(cmd);

    return ret;
}

int nexcacheClusterAppendCommand(nexcacheClusterContext *cc, const char *format,
                               ...) {

    int ret;
    va_list ap;

    if (cc == NULL || format == NULL) {
        return NEXCACHE_ERR;
    }

    va_start(ap, format);
    ret = nexcacheClustervAppendCommand(cc, format, ap);
    va_end(ap);

    return ret;
}

int nexcacheClustervAppendCommandToNode(nexcacheClusterContext *cc,
                                      nexcacheClusterNode *node,
                                      const char *format, va_list ap) {
    nexcacheContext *c;
    struct cmd *command = NULL;
    char *cmd = NULL;
    int len;

    c = nexcacheClusterGetNexCacheContext(cc, node);
    if (c == NULL) {
        return NEXCACHE_ERR;
    } else if (c->err) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        return NEXCACHE_ERR;
    }

    len = nexcachevFormatCommand(&cmd, format, ap);

    if (len == -1) {
        goto oom;
    } else if (len == -2) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER, "Invalid format string");
        return NEXCACHE_ERR;
    }

    // Append the command to the outgoing nexcache buffer
    if (nexcacheAppendFormattedCommand(c, cmd, len) != NEXCACHE_OK) {
        nexcacheClusterSetError(cc, c->err, c->errstr);
        vk_free(cmd);
        return NEXCACHE_ERR;
    }

    // Keep the command in the outstanding request list
    command = command_get();
    if (command == NULL) {
        vk_free(cmd);
        goto oom;
    }
    command->cmd = cmd;
    command->clen = len;
    command->node_addr = sdsnew(node->addr);
    if (command->node_addr == NULL)
        goto oom;

    if (listAddNodeTail(cc->requests, command) == NULL)
        goto oom;

    return NEXCACHE_OK;

oom:
    command_destroy(command);
    nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
    return NEXCACHE_ERR;
}

int nexcacheClusterAppendCommandToNode(nexcacheClusterContext *cc,
                                     nexcacheClusterNode *node,
                                     const char *format, ...) {
    int ret;
    va_list ap;

    if (cc == NULL || node == NULL || format == NULL) {
        return NEXCACHE_ERR;
    }

    va_start(ap, format);
    ret = nexcacheClustervAppendCommandToNode(cc, node, format, ap);
    va_end(ap);

    return ret;
}

int nexcacheClusterAppendCommandArgv(nexcacheClusterContext *cc, int argc,
                                   const char **argv, const size_t *argvlen) {
    int ret;
    char *cmd;
    int len;

    len = nexcacheFormatCommandArgv(&cmd, argc, argv, argvlen);
    if (len == -1) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAppendFormattedCommand(cc, cmd, len);

    vk_free(cmd);

    return ret;
}

NEXCACHE_UNUSED
static int nexcacheClusterSendAll(nexcacheClusterContext *cc) {
    dictEntry *de;
    nexcacheClusterNode *node;
    nexcacheContext *c = NULL;
    int wdone = 0;

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    while ((de = dictNext(&di)) != NULL) {
        node = dictGetVal(de);
        if (node == NULL) {
            continue;
        }

        c = nexcacheClusterGetNexCacheContext(cc, node);
        if (c == NULL) {
            continue;
        }

        /* Write until done */
        do {
            if (nexcacheBufferWrite(c, &wdone) == NEXCACHE_ERR) {
                return NEXCACHE_ERR;
            }
        } while (!wdone);
    }

    return NEXCACHE_OK;
}

NEXCACHE_UNUSED
static int nexcacheClusterClearAll(nexcacheClusterContext *cc) {
    dictEntry *de;
    nexcacheClusterNode *node;
    nexcacheContext *c = NULL;

    if (cc == NULL) {
        return NEXCACHE_ERR;
    }

    nexcacheClusterClearError(cc);

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    while ((de = dictNext(&di)) != NULL) {
        node = dictGetVal(de);
        if (node == NULL) {
            continue;
        }

        c = node->con;
        if (c == NULL) {
            continue;
        }

        nexcacheFree(c);
        node->con = NULL;
    }

    return NEXCACHE_OK;
}

int nexcacheClusterGetReply(nexcacheClusterContext *cc, void **reply) {
    struct cmd *command;
    listNode *list_command;
    int slot_num;

    if (cc == NULL || reply == NULL)
        return NEXCACHE_ERR;

    nexcacheClusterClearError(cc);
    *reply = NULL;

    list_command = listFirst(cc->requests);

    /* No queued requests. */
    if (list_command == NULL) {
        *reply = NULL;
        return NEXCACHE_OK;
    }

    command = list_command->value;
    if (command == NULL) {
        nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                              "command in the requests list is null");
        goto error;
    }

    /* Get reply when the command was sent via slot */
    slot_num = command->slot_num;
    if (slot_num >= 0) {
        nexcacheClusterNode *node;
        if ((node = node_get_by_table(cc, (uint32_t)slot_num)) == NULL)
            goto error;

        listDelNode(cc->requests, list_command);
        return nexcacheClusterGetReplyFromNode(cc, node, reply);
    }
    /* Get reply when the command was sent to a given node */
    if (command->node_addr != NULL) {
        dictEntry *de = dictFind(cc->nodes, command->node_addr);
        if (de == NULL) {
            nexcacheClusterSetError(cc, NEXCACHE_ERR_OTHER,
                                  "command was sent to a now unknown node");
            goto error;
        }

        listDelNode(cc->requests, list_command);
        return nexcacheClusterGetReplyFromNode(cc, dictGetVal(de), reply);
    }

error:
    listDelNode(cc->requests, list_command);
    return NEXCACHE_ERR;
}

/**
 * Resets cluster state after pipeline.
 * Resets NexCache node connections if pipeline commands were not called beforehand.
 */
void nexcacheClusterReset(nexcacheClusterContext *cc) {
    int status;
    void *reply;

    if (cc == NULL) {
        return;
    }

    if (cc->err) {
        nexcacheClusterClearAll(cc);
    } else {
        /* Write/flush each nodes output buffer to socket */
        nexcacheClusterSendAll(cc);

        /* Expect a reply for each pipelined request */
        do {
            status = nexcacheClusterGetReply(cc, &reply);
            if (status == NEXCACHE_OK) {
                freeReplyObject(reply);
            } else {
                nexcacheClusterClearAll(cc);
                break;
            }
        } while (reply != NULL);
    }

    listIter li;
    listRewind(cc->requests, &li);
    listNode *ln;
    while ((ln = listNext(&li))) {
        listDelNode(cc->requests, ln);
    }

    if (cc->need_update_route) {
        status = nexcacheClusterUpdateSlotmap(cc);
        if (status != NEXCACHE_OK) {
            /* Specific error already set */
            return;
        }
        cc->need_update_route = 0;
    }
}

/*############nexcache cluster async############*/

static void nexcacheClusterAsyncSetError(nexcacheClusterAsyncContext *acc, int type,
                                       const char *str) {
    nexcacheClusterSetError(&acc->cc, type, str); /* Keep error flags identical. */
    acc->err = acc->cc.err;
}

static inline void nexcacheClusterAsyncClearError(nexcacheClusterAsyncContext *acc) {
    nexcacheClusterClearError(&acc->cc);
    acc->err = acc->cc.err;
}

static cluster_async_data *cluster_async_data_create(void) {
    /* use calloc to guarantee all fields are zeroed */
    return vk_calloc(1, sizeof(cluster_async_data));
}

static void cluster_async_data_free(cluster_async_data *cad) {
    if (cad == NULL) {
        return;
    }

    command_destroy(cad->command);

    vk_free(cad);
}

static void unlinkAsyncContextAndNode(void *data) {
    nexcacheClusterNode *node;

    if (data) {
        node = (nexcacheClusterNode *)(data);
        node->acon = NULL;
    }
}

/* Reply callback function for SELECT */
void selectReplyCallback(nexcacheAsyncContext *ac, void *r, void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    nexcacheClusterAsyncContext *acc = (nexcacheClusterAsyncContext *)privdata;

    if (reply == NULL || reply->type == NEXCACHE_REPLY_ERROR) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                   "Failed to select logical database");
        nexcacheAsyncDisconnect(ac);
    }
}

nexcacheAsyncContext *
nexcacheClusterGetNexCacheAsyncContext(nexcacheClusterAsyncContext *acc,
                                   nexcacheClusterNode *node) {
    nexcacheAsyncContext *ac;
    int ret;

    if (node == NULL) {
        return NULL;
    }

    ac = node->acon;
    if (ac != NULL) {
        if (ac->c.err == 0) {
            return ac;
        } else {
            /* The cluster node has a nexcache context with errors. Libnexcache
             * will asynchronously destruct the context and unlink it from
             * the cluster node object. Return an error until done.
             * An example scenario is when sending a command from a command
             * callback, which has a NULL reply due to a disconnect. */
            nexcacheClusterAsyncSetError(acc, ac->c.err, ac->c.errstr);
            return NULL;
        }
    }

    // No async context exists, perform a connect

    if (node->host == NULL || node->port <= 0) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                   "node host or port is error");
        return NULL;
    }

    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, node->host, node->port);
    options.connect_timeout = acc->cc.connect_timeout;
    options.command_timeout = acc->cc.command_timeout;
    options.options = acc->cc.options;

    node->lastConnectionAttempt = vk_usec_now();

    ac = nexcacheAsyncConnectWithOptions(&options);
    if (ac == NULL) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OOM, "Out of memory");
        return NULL;
    }

    if (ac->err) {
        nexcacheClusterAsyncSetError(acc, ac->err, ac->errstr);
        nexcacheAsyncFree(ac);
        return NULL;
    }

    if (acc->cc.tls &&
        acc->cc.tls_init_fn(&ac->c, acc->cc.tls) != NEXCACHE_OK) {
        nexcacheClusterAsyncSetError(acc, ac->c.err, ac->c.errstr);
        nexcacheAsyncFree(ac);
        return NULL;
    }

    // Authenticate when needed
    if (acc->cc.password != NULL) {
        if (acc->cc.username != NULL) {
            ret = nexcacheAsyncCommand(ac, NULL, NULL, "AUTH %s %s",
                                     acc->cc.username, acc->cc.password);
        } else {
            ret = nexcacheAsyncCommand(ac, NULL, NULL, "AUTH %s",
                                     acc->cc.password);
        }

        if (ret != NEXCACHE_OK) {
            nexcacheClusterAsyncSetError(acc, ac->c.err, ac->c.errstr);
            nexcacheAsyncFree(ac);
            return NULL;
        }
    }
    // Select logical database when needed
    if (acc->cc.select_db > 0) {
        ret = nexcacheAsyncCommand(ac, selectReplyCallback, acc, "SELECT %d", acc->cc.select_db);
        if (ret != NEXCACHE_OK) {
            nexcacheClusterAsyncSetError(acc, ac->c.err, ac->c.errstr);
            nexcacheAsyncFree(ac);
            return NULL;
        }
    }

    if (acc->attach_fn) {
        ret = acc->attach_fn(ac, acc->attach_data);
        if (ret != NEXCACHE_OK) {
            nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                       "Failed to attach event adapter");
            nexcacheAsyncFree(ac);
            return NULL;
        }
    }

    if (acc->onConnect) {
        nexcacheAsyncSetConnectCallback(ac, acc->onConnect);
    }

    if (acc->onDisconnect) {
        nexcacheAsyncSetDisconnectCallback(ac, acc->onDisconnect);
    }

    ac->data = node;
    ac->dataCleanup = unlinkAsyncContextAndNode;
    node->acon = ac;

    return ac;
}

static int nexcacheClusterAsyncContextInit(nexcacheClusterAsyncContext *acc,
                                         const nexcacheClusterOptions *options) {
    /* Setup errstr to point to common error string in nexcacheClusterContext. */
    acc->errstr = acc->cc.errstr;

    if (nexcacheClusterContextInit(&acc->cc, options) != NEXCACHE_OK) {
        nexcacheClusterAsyncSetError(acc, acc->cc.err, acc->cc.errstr);
        return NEXCACHE_ERR;
    }

    if (options->async_connect_callback != NULL) {
        acc->onConnect = options->async_connect_callback;
    }
    if (options->async_disconnect_callback != NULL) {
        acc->onDisconnect = options->async_disconnect_callback;
    }
    if (options->attach_fn == NULL) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                   "No event library configured");
        return NEXCACHE_ERR;
    }
    acc->attach_fn = options->attach_fn;
    acc->attach_data = options->attach_data;
    return NEXCACHE_OK;
}

nexcacheClusterAsyncContext *nexcacheClusterAsyncConnectWithOptions(const nexcacheClusterOptions *options) {
    nexcacheClusterAsyncContext *acc;

    acc = vk_calloc(1, sizeof(nexcacheClusterAsyncContext));
    if (acc == NULL)
        return NULL;

    if (nexcacheClusterAsyncContextInit(acc, options) == NEXCACHE_OK) {
        /* Only connect if options are ok. */
        nexcacheClusterAsyncConnect(acc);
    }

    return acc;
}

static int nexcacheClusterAsyncConnect(nexcacheClusterAsyncContext *acc) {
    /* Use blocking initial slotmap update when configured. */
    if (acc->cc.flags & NEXCACHE_FLAG_BLOCKING_INITIAL_UPDATE) {
        if (nexcacheClusterUpdateSlotmap(&acc->cc) != NEXCACHE_OK) {
            nexcacheClusterAsyncSetError(acc, acc->cc.err, acc->cc.errstr);
            return NEXCACHE_ERR;
        }

        /* Disconnect any non-async context used for the initial update. */
        dictIterator di;
        dictInitIterator(&di, acc->cc.nodes);
        dictEntry *de;
        while ((de = dictNext(&di)) != NULL) {
            nexcacheClusterNode *node = dictGetVal(de);
            if (node->con) {
                nexcacheFree(node->con);
                node->con = NULL;
            }
        }

        return NEXCACHE_OK;
    }
    /* Use non-blocking initial slotmap update. */
    return updateSlotMapAsync(acc, NULL /*any node*/);
}

/* Reply callback function for CLUSTER SLOTS */
void clusterSlotsReplyCallback(nexcacheAsyncContext *ac, void *r,
                               void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    nexcacheClusterAsyncContext *acc = (nexcacheClusterAsyncContext *)privdata;
    acc->lastSlotmapUpdateAttempt = vk_usec_now();

    if (reply == NULL) {
        /* Retry using available nodes */
        updateSlotMapAsync(acc, NULL);
        return;
    }

    nexcacheClusterContext *cc = &acc->cc;
    dict *nodes = parse_cluster_slots(cc, &ac->c, reply);
    if (updateNodesAndSlotmap(cc, nodes) != NEXCACHE_OK) {
        /* Retry using available nodes */
        updateSlotMapAsync(acc, NULL);
    }
}

/* Reply callback function for CLUSTER NODES */
void clusterNodesReplyCallback(nexcacheAsyncContext *ac, void *r,
                               void *privdata) {
    nexcacheReply *reply = (nexcacheReply *)r;
    nexcacheClusterAsyncContext *acc = (nexcacheClusterAsyncContext *)privdata;
    acc->lastSlotmapUpdateAttempt = vk_usec_now();

    if (reply == NULL) {
        /* Retry using available nodes */
        updateSlotMapAsync(acc, NULL);
        return;
    }

    nexcacheClusterContext *cc = &acc->cc;
    dict *nodes = parse_cluster_nodes(cc, &ac->c, reply);
    if (updateNodesAndSlotmap(cc, nodes) != NEXCACHE_OK) {
        /* Retry using available nodes */
        updateSlotMapAsync(acc, NULL);
    }
}

#define nodeIsConnected(n)                       \
    ((n)->acon != NULL && (n)->acon->err == 0 && \
     (n)->acon->c.flags & NEXCACHE_CONNECTED)

/* Select a node.
 * Primarily selects a connected node found close to a randomly picked index of
 * all known nodes. The random index should give a more even distribution of
 * selected nodes. If no connected node is found while iterating to this index
 * the remaining nodes are also checked until a connected node is found.
 * If no connected node is found a node for which a connect has not been attempted
 * within throttle-time, and is found near the picked index, is selected.
 */
static nexcacheClusterNode *selectNode(dict *nodes) {
    nexcacheClusterNode *node, *selected = NULL;
    dictIterator di;
    dictInitIterator(&di, nodes);

    int64_t throttleLimit = vk_usec_now() - SLOTMAP_UPDATE_THROTTLE_USEC;
    unsigned long currentIndex = 0;
    unsigned long checkIndex = random() % dictSize(nodes);

    dictEntry *de;
    while ((de = dictNext(&di)) != NULL) {
        node = dictGetVal(de);

        if (nodeIsConnected(node)) {
            /* Keep any connected node */
            selected = node;
        } else if (node->lastConnectionAttempt < throttleLimit &&
                   (selected == NULL || (currentIndex < checkIndex &&
                                         !nodeIsConnected(selected)))) {
            /* Keep an accepted node when none is yet found, or
               any accepted node until the chosen index is reached */
            selected = node;
        }

        /* Return a found connected node when chosen index is reached. */
        if (currentIndex >= checkIndex && selected != NULL &&
            nodeIsConnected(selected))
            break;
        currentIndex += 1;
    }
    return selected;
}

/* Update the slot map by querying a selected cluster node. If ac is NULL, an
 * arbitrary connected node is selected. */
static int updateSlotMapAsync(nexcacheClusterAsyncContext *acc,
                              nexcacheAsyncContext *ac) {
    if (acc->lastSlotmapUpdateAttempt == SLOTMAP_UPDATE_ONGOING) {
        /* Don't allow concurrent slot map updates. */
        return NEXCACHE_ERR;
    }
    if (acc->cc.flags & NEXCACHE_FLAG_DISCONNECTING) {
        /* No slot map updates during a cluster client disconnect. */
        return NEXCACHE_ERR;
    }

    if (ac == NULL) {
        nexcacheClusterNode *node = selectNode(acc->cc.nodes);
        if (node == NULL) {
            goto error;
        }

        /* Get libnexcache context, connect if needed */
        ac = nexcacheClusterGetNexCacheAsyncContext(acc, node);
    }
    if (ac == NULL)
        goto error; /* Specific error already set */

    /* Send a command depending of config */
    int status;
    if (acc->cc.flags & NEXCACHE_FLAG_USE_CLUSTER_NODES) {
        status = nexcacheAsyncCommand(ac, clusterNodesReplyCallback, acc,
                                    NEXCACHE_COMMAND_CLUSTER_NODES);
    } else {
        status = nexcacheAsyncCommand(ac, clusterSlotsReplyCallback, acc,
                                    NEXCACHE_COMMAND_CLUSTER_SLOTS);
    }

    if (status == NEXCACHE_OK) {
        acc->lastSlotmapUpdateAttempt = SLOTMAP_UPDATE_ONGOING;
        return NEXCACHE_OK;
    }

error:
    acc->lastSlotmapUpdateAttempt = vk_usec_now();
    return NEXCACHE_ERR;
}

/* Start a slotmap update if the throttling allows. */
static void throttledUpdateSlotMapAsync(nexcacheClusterAsyncContext *acc,
                                        nexcacheAsyncContext *ac) {
    if (acc->lastSlotmapUpdateAttempt != SLOTMAP_UPDATE_ONGOING &&
        (acc->lastSlotmapUpdateAttempt + SLOTMAP_UPDATE_THROTTLE_USEC) <
            vk_usec_now()) {
        updateSlotMapAsync(acc, ac);
    }
}

static void nexcacheClusterAsyncCallback(nexcacheAsyncContext *ac, void *r,
                                       void *privdata) {
    int ret;
    nexcacheReply *reply = r;
    cluster_async_data *cad = privdata;
    nexcacheClusterAsyncContext *acc;
    nexcacheClusterContext *cc;
    nexcacheAsyncContext *ac_retry = NULL;
    nexcacheClusterNode *node;
    struct cmd *command;

    if (cad == NULL) {
        goto error;
    }

    acc = cad->acc;
    if (acc == NULL) {
        goto error;
    }

    cc = &acc->cc;
    if (cc == NULL) {
        goto error;
    }

    command = cad->command;
    if (command == NULL) {
        goto error;
    }

    if (reply == NULL) {
        /* Copy reply specific error from libnexcache */
        nexcacheClusterAsyncSetError(acc, ac->err, ac->errstr);

        node = (nexcacheClusterNode *)ac->data;
        if (node == NULL)
            goto done; /* Node already removed from topology */

        /* Start a slotmap update when the throttling allows */
        throttledUpdateSlotMapAsync(acc, NULL);
        goto done;
    }

    /* Skip retry handling when not expected, or during a client disconnect. */
    if (cad->retry_count == NO_RETRY || cc->flags & NEXCACHE_FLAG_DISCONNECTING)
        goto done;

    replyErrorType error_type = getReplyErrorType(reply);
    if (error_type > CLUSTER_NO_ERROR && error_type < CLUSTER_ERR_OTHER) {
        cad->retry_count++;
        if (cad->retry_count > cc->max_retry_count) {
            cad->retry_count = 0;
            nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_CLUSTER_TOO_MANY_RETRIES,
                                       "too many cluster retries");
            goto done;
        }

        int slot = -1;
        switch (error_type) {
        case CLUSTER_ERR_MOVED:
            /* Initiate slot mapping update using the node that sent MOVED. */
            throttledUpdateSlotMapAsync(acc, ac);

            node = getNodeFromRedirectReply(cc, &ac->c, reply, &slot);
            if (node == NULL) {
                nexcacheClusterAsyncSetError(acc, cc->err, cc->errstr);
                goto done;
            }
            /* Update the slot mapping entry for this slot. */
            if (slot >= 0) {
                cc->table[slot] = node;
            }
            ac_retry = nexcacheClusterGetNexCacheAsyncContext(acc, node);

            break;
        case CLUSTER_ERR_ASK:
            node = getNodeFromRedirectReply(cc, &ac->c, reply, NULL);
            if (node == NULL) {
                nexcacheClusterAsyncSetError(acc, cc->err, cc->errstr);
                goto done;
            }

            ac_retry = nexcacheClusterGetNexCacheAsyncContext(acc, node);
            if (ac_retry == NULL) {
                /* Specific error already set */
                goto done;
            }

            ret =
                nexcacheAsyncCommand(ac_retry, NULL, NULL, NEXCACHE_COMMAND_ASKING);
            if (ret != NEXCACHE_OK) {
                goto error;
            }

            break;
        case CLUSTER_ERR_TRYAGAIN:
        case CLUSTER_ERR_CLUSTERDOWN:
            ac_retry = ac;

            break;
        default:

            goto done;
        }

        goto retry;
    }

done:

    if (acc->err) {
        cad->callback(acc, NULL, cad->privdata);
    } else {
        cad->callback(acc, r, cad->privdata);
    }

    nexcacheClusterAsyncClearError(acc);

    cluster_async_data_free(cad);

    return;

retry:

    ret = nexcacheAsyncFormattedCommand(ac_retry, nexcacheClusterAsyncCallback, cad,
                                      command->cmd, command->clen);
    if (ret != NEXCACHE_OK) {
        goto error;
    }

    return;

error:

    cluster_async_data_free(cad);
}

int nexcacheClusterAsyncFormattedCommand(nexcacheClusterAsyncContext *acc,
                                       nexcacheClusterCallbackFn *fn,
                                       void *privdata, char *cmd, int len) {

    nexcacheClusterContext *cc;
    int status = NEXCACHE_OK;
    nexcacheClusterNode *node;
    nexcacheAsyncContext *ac;
    struct cmd *command = NULL;
    cluster_async_data *cad = NULL;

    if (acc == NULL) {
        return NEXCACHE_ERR;
    }

    cc = &acc->cc;

    /* Don't accept new commands when the client is about to disconnect. */
    if (cc->flags & NEXCACHE_FLAG_DISCONNECTING) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER, "disconnecting");
        return NEXCACHE_ERR;
    }

    nexcacheClusterAsyncClearError(acc);

    command = command_get();
    if (command == NULL) {
        goto oom;
    }

    command->cmd = vk_calloc(len, sizeof(*command->cmd));
    if (command->cmd == NULL) {
        goto oom;
    }
    memcpy(command->cmd, cmd, len);
    command->clen = len;

    if (prepareCommand(cc, command) != NEXCACHE_OK) {
        nexcacheClusterAsyncSetError(acc, cc->err, cc->errstr);
        goto error;
    }

    node = node_get_by_table(cc, (uint32_t)command->slot_num);
    if (node == NULL) {
        /* Initiate a slotmap update since the slot is not served. */
        throttledUpdateSlotMapAsync(acc, NULL);

        /* node_get_by_table() has set the error on cc. */
        nexcacheClusterAsyncSetError(acc, cc->err, cc->errstr);
        goto error;
    }

    ac = nexcacheClusterGetNexCacheAsyncContext(acc, node);
    if (ac == NULL) {
        /* Specific error already set */
        goto error;
    }

    cad = cluster_async_data_create();
    if (cad == NULL) {
        goto oom;
    }

    cad->acc = acc;
    cad->command = command;
    command = NULL; /* Memory ownership moved. */
    cad->callback = fn;
    cad->privdata = privdata;

    status = nexcacheAsyncFormattedCommand(ac, nexcacheClusterAsyncCallback, cad,
                                         cmd, len);
    if (status != NEXCACHE_OK) {
        nexcacheClusterAsyncSetError(acc, ac->err, ac->errstr);
        goto error;
    }
    return NEXCACHE_OK;

oom:
    nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OOM, "Out of memory");
    // passthrough

error:
    cluster_async_data_free(cad);
    command_destroy(command);
    return NEXCACHE_ERR;
}

int nexcacheClusterAsyncFormattedCommandToNode(nexcacheClusterAsyncContext *acc,
                                             nexcacheClusterNode *node,
                                             nexcacheClusterCallbackFn *fn,
                                             void *privdata, char *cmd,
                                             int len) {
    nexcacheClusterContext *cc = &acc->cc;
    nexcacheAsyncContext *ac;
    int status;
    cluster_async_data *cad = NULL;
    struct cmd *command = NULL;

    /* Don't accept new commands when the client is about to disconnect. */
    if (cc->flags & NEXCACHE_FLAG_DISCONNECTING) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER, "disconnecting");
        return NEXCACHE_ERR;
    }

    ac = nexcacheClusterGetNexCacheAsyncContext(acc, node);
    if (ac == NULL) {
        /* Specific error already set */
        return NEXCACHE_ERR;
    }

    nexcacheClusterAsyncClearError(acc);

    command = command_get();
    if (command == NULL) {
        goto oom;
    }

    command->cmd = vk_calloc(len, sizeof(*command->cmd));
    if (command->cmd == NULL) {
        goto oom;
    }
    memcpy(command->cmd, cmd, len);
    command->clen = len;

    cad = cluster_async_data_create();
    if (cad == NULL)
        goto oom;

    cad->acc = acc;
    cad->command = command;
    command = NULL; /* Memory ownership moved. */
    cad->callback = fn;
    cad->privdata = privdata;
    cad->retry_count = NO_RETRY;

    status = nexcacheAsyncFormattedCommand(ac, nexcacheClusterAsyncCallback, cad,
                                         cmd, len);
    if (status != NEXCACHE_OK) {
        nexcacheClusterAsyncSetError(acc, ac->err, ac->errstr);
        goto error;
    }

    return NEXCACHE_OK;

oom:
    nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER, "Out of memory");
    // passthrough

error:
    cluster_async_data_free(cad);
    command_destroy(command);
    return NEXCACHE_ERR;
}

int nexcacheClustervAsyncCommand(nexcacheClusterAsyncContext *acc,
                               nexcacheClusterCallbackFn *fn, void *privdata,
                               const char *format, va_list ap) {
    int ret;
    char *cmd;
    int len;

    if (acc == NULL) {
        return NEXCACHE_ERR;
    }

    len = nexcachevFormatCommand(&cmd, format, ap);
    if (len == -1) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    } else if (len == -2) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                   "Invalid format string");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAsyncFormattedCommand(acc, fn, privdata, cmd, len);

    vk_free(cmd);

    return ret;
}

int nexcacheClusterAsyncCommand(nexcacheClusterAsyncContext *acc,
                              nexcacheClusterCallbackFn *fn, void *privdata,
                              const char *format, ...) {
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = nexcacheClustervAsyncCommand(acc, fn, privdata, format, ap);
    va_end(ap);

    return ret;
}

int nexcacheClusterAsyncCommandToNode(nexcacheClusterAsyncContext *acc,
                                    nexcacheClusterNode *node,
                                    nexcacheClusterCallbackFn *fn, void *privdata,
                                    const char *format, ...) {
    int ret;
    va_list ap;
    int len;
    char *cmd = NULL;

    /* Allocate cmd and encode the variadic command */
    va_start(ap, format);
    len = nexcachevFormatCommand(&cmd, format, ap);
    va_end(ap);

    if (len == -1) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER, "Out of memory");
        return NEXCACHE_ERR;
    } else if (len == -2) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OTHER,
                                   "Invalid format string");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAsyncFormattedCommandToNode(acc, node, fn, privdata, cmd,
                                                   len);
    vk_free(cmd);
    return ret;
}

int nexcacheClusterAsyncCommandArgv(nexcacheClusterAsyncContext *acc,
                                  nexcacheClusterCallbackFn *fn, void *privdata,
                                  int argc, const char **argv,
                                  const size_t *argvlen) {
    int ret;
    char *cmd;
    int len;

    len = nexcacheFormatCommandArgv(&cmd, argc, argv, argvlen);
    if (len == -1) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAsyncFormattedCommand(acc, fn, privdata, cmd, len);

    vk_free(cmd);

    return ret;
}

int nexcacheClusterAsyncCommandArgvToNode(nexcacheClusterAsyncContext *acc,
                                        nexcacheClusterNode *node,
                                        nexcacheClusterCallbackFn *fn,
                                        void *privdata, int argc,
                                        const char **argv,
                                        const size_t *argvlen) {

    int ret;
    char *cmd;
    int len;

    len = nexcacheFormatCommandArgv(&cmd, argc, argv, argvlen);
    if (len == -1) {
        nexcacheClusterAsyncSetError(acc, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    ret = nexcacheClusterAsyncFormattedCommandToNode(acc, node, fn, privdata, cmd,
                                                   len);

    vk_free(cmd);

    return ret;
}

void nexcacheClusterAsyncDisconnect(nexcacheClusterAsyncContext *acc) {
    nexcacheClusterContext *cc;
    nexcacheAsyncContext *ac;
    dictEntry *de;
    nexcacheClusterNode *node;

    if (acc == NULL) {
        return;
    }

    cc = &acc->cc;
    cc->flags |= NEXCACHE_FLAG_DISCONNECTING;

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    while ((de = dictNext(&di)) != NULL) {
        node = dictGetVal(de);

        ac = node->acon;

        if (ac == NULL) {
            continue;
        }

        nexcacheAsyncDisconnect(ac);
    }
}

void nexcacheClusterAsyncFree(nexcacheClusterAsyncContext *acc) {
    if (acc == NULL)
        return;

    nexcacheClusterContext *cc = &acc->cc;
    cc->flags |= NEXCACHE_FLAG_DISCONNECTING;
    nexcacheClusterFree(cc);
}

struct nodeIterator {
    uint64_t route_version;
    nexcacheClusterContext *cc;
    int retries_left;
    dictIterator di;
};
/* Make sure the opaque memory blob can contain a nodeIterator. */
vk_static_assert(sizeof(nexcacheClusterNodeIterator) >= sizeof(struct nodeIterator));

/* Initiate an iterator for iterating over current cluster nodes */
void nexcacheClusterInitNodeIterator(nexcacheClusterNodeIterator *iter,
                                   nexcacheClusterContext *cc) {
    struct nodeIterator *ni = (struct nodeIterator *)iter;
    ni->cc = cc;
    ni->route_version = cc->route_version;
    dictInitIterator(&ni->di, cc->nodes);
    ni->retries_left = 1;
}

/* Get next node from the iterator
 * The iterator will restart if the routing table is updated
 * before all nodes have been iterated. */
nexcacheClusterNode *nexcacheClusterNodeNext(nexcacheClusterNodeIterator *iter) {
    struct nodeIterator *ni = (struct nodeIterator *)iter;
    if (ni->retries_left <= 0)
        return NULL;

    if (ni->route_version != ni->cc->route_version) {
        // The routing table has changed and current iterator
        // is invalid. The nodes dict has been recreated in
        // the cluster context. We need to re-init the dictIter.
        dictInitIterator(&ni->di, ni->cc->nodes);
        ni->route_version = ni->cc->route_version;
        ni->retries_left--;
    }

    dictEntry *de;
    if ((de = dictNext(&ni->di)) != NULL)
        return dictGetVal(de);
    else
        return NULL;
}

/* Get hash slot for given key string, which can include hash tags */
unsigned int nexcacheClusterGetSlotByKey(char *key) {
    return keyHashSlot(key, strlen(key));
}

/* Get node that handles given key string, which can include hash tags */
nexcacheClusterNode *nexcacheClusterGetNodeByKey(nexcacheClusterContext *cc,
                                             char *key) {
    return node_get_by_table(cc, keyHashSlot(key, strlen(key)));
}
