#ifndef VERA_LOCKFREE_H
#define VERA_LOCKFREE_H

#include <stdatomic.h>
#include <stddef.h>

/* NEX-VERA: Vyukov Lock-Free MPSC Queue 
 * Implementation for Multi-Producer Single-Consumer communication. */

typedef struct mpsc_node {
    _Atomic(struct mpsc_node *) next;
    void *data;
} __attribute__((aligned(256))) mpsc_node_t;

typedef struct {
    _Atomic(mpsc_node_t *) head;
    mpsc_node_t *tail;
    mpsc_node_t stub;
} __attribute__((aligned(256))) mpsc_queue_t;

static inline void mpsc_init(mpsc_queue_t *q) {
    q->head = &q->stub;
    q->tail = &q->stub;
    atomic_store(&q->stub.next, NULL);
}

static inline void mpsc_enqueue(mpsc_queue_t *q, mpsc_node_t *n) {
    atomic_store(&n->next, NULL);
    mpsc_node_t *prev = atomic_exchange(&q->head, n);
    atomic_store(&prev->next, n);
}

static inline mpsc_node_t *mpsc_dequeue(mpsc_queue_t *q) {
    mpsc_node_t *tail = q->tail;
    mpsc_node_t *next = atomic_load(&tail->next);
    if (tail == &q->stub) {
        if (next == NULL) return NULL;
        q->tail = next;
        tail = next;
        next = atomic_load(&next->next);
    }
    if (next) {
        q->tail = next;
        return tail;
    }
    mpsc_node_t *head = atomic_load(&q->head);
    if (tail != head) return NULL;
    mpsc_enqueue(q, &q->stub);
    next = atomic_load(&tail->next);
    if (next) {
        q->tail = next;
        return tail;
    }
    return NULL;
}

#endif
