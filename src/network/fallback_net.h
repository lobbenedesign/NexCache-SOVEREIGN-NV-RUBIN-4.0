#ifndef NEXCACHE_FALLBACK_NET_H
#define NEXCACHE_FALLBACK_NET_H

#include "net_backend.h"

/* The Auto-Fallback Backend intelligently selects the best available
 * networking mechanism. It tries RDMA/DPDK first. If the hardware is
 * not present, or if the client connects via standard TCP (detected
 * dynamically), it falls back silently to io_uring or epoll.
 *
 * This fulfills the NexCache v5.0 "Auto-Fallback Protocol" requirement,
 * ensuring 100% drop-in compatibility for standard Redis clients.
 */

extern const NetBackend AutoFallbackBackend;

#endif /* NEXCACHE_FALLBACK_NET_H */

// Nuova implementazione TCP Esplicito
typedef struct NexListener {
    int            fd;           // socket fd (TCP) o completion channel fd (RDMA)
    int backend;
    uint16_t       port;
    bool           tls_enabled;
} NexListener;

int nexcache_init_listeners(void *cfg);
