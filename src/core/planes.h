#ifndef NEXCACHE_PLANES_H
#define NEXCACHE_PLANES_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Thread plane enumeration
 * As inspired by Pelikan Data/Control plane separation.
 * Performance-sensitive and non-performance-sensitive paths do not contend.
 */
typedef enum {
    PLANE_DATA,
    PLANE_ADMIN,
    PLANE_SNAPSHOT,
    PLANE_REPLICATION,
    PLANE_TIERING,
    PLANE_OBSERVABILITY,
    PLANE_EXPIRATION,
    PLANE_COUNT /* Total number of planes */
} ThreadPlane;

/**
 * @brief Initialize a thread plane and set its scheduling properties.
 *
 * @param plane The plane to initialize
 * @param cpu_core CPU affinity core (-1 for no affinity)
 * @param priority RT priority (SCHED_FIFO), 0 for normal SCHED_OTHER
 */
void plane_init(ThreadPlane plane, int cpu_core, int priority);

/**
 * @brief Send a cross-plane message lock-free.
 * Data plane communicates with control plane via non-blocking rings.
 *
 * @param from Source plane
 * @param to Destination plane
 * @param msg Pointer to message data
 * @param msg_size Size of message data
 */
void plane_send_message(ThreadPlane from, ThreadPlane to, void *msg, size_t msg_size);

#endif /* NEXCACHE_PLANES_H */
