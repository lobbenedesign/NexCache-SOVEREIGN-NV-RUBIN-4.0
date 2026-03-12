/* NexCache Dashboard — Header
 * Copyright (c) 2026 NexCache Project — BSD License
 */
#ifndef NEXCACHE_DASHBOARD_H
#define NEXCACHE_DASHBOARD_H

#include <stdint.h>
#include <stddef.h>

#define DASHBOARD_PORT_DEFAULT 8080

typedef struct DashboardConfig {
    int port;
    int enable_auth;
    char auth_token[64];
} DashboardConfig;

/* Info aggregata esposta dal dashboard */
typedef struct NexDashboardInfo {
    /* Comandi */
    uint64_t total_commands;
    uint64_t commands_per_sec;
    uint64_t total_hits;
    uint64_t total_misses;
    double hit_rate;
    /* Latenza */
    double latency_p50_us;
    double latency_p99_us;
    double latency_p999_us;
    double latency_max_us;
    /* Network */
    uint32_t connected_clients;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    /* Memoria */
    uint64_t used_memory_bytes;
    /* Vector */
    uint64_t vector_searches;
    double hnsw_avg_search_us;
    /* Raft */
    char raft_role[16]; /* "leader"/"follower"/"candidate" */
    uint64_t raft_term;
    uint32_t raft_node_id;
    /* Timing */
    uint64_t start_time_us;
} NexDashboardInfo;

/* ── API ────────────────────────────────────────────────────── */
int dashboard_init(int port, DashboardConfig *cfg);
void dashboard_update(const NexDashboardInfo *info);
void dashboard_shutdown(void);

#endif /* NEXCACHE_DASHBOARD_H */
