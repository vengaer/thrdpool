#ifndef THRDPOOL_H
#define THRDPOOL_H

#include "proc.h"
#include "procq.h"

#include <stdbool.h>
#include <stddef.h>

#include <pthread.h>

struct thrdpool {
    bool join;
    pthread_cond_t cv;
    pthread_mutex_t lock;
    size_t size;
    struct thrdpool_procq q;
    pthread_t workers[];
};

#define thrdpool_bytesize(capacity) \
    (sizeof(struct thrdpool) + capacity * sizeof(((struct thrdpool *)0)->workers[0]))

#define thrdpool_init(capacity)     \
    thrdpool_init_impl(&(union { unsigned char _[thrdpool_bytesize(capacity)]; struct thrdpool pool; }){ 0 }.pool, capacity)

struct thrdpool *thrdpool_init_impl(struct thrdpool *pool, size_t capacity);
bool thrdpool_schedule(struct thrdpool *restrict pool, struct thrdpool_proc const *restrict proc);

inline bool thrdpool_destroy(struct thrdpool *pool) {
    extern bool thrdpool_destroy_internal(struct thrdpool *pool, size_t size);
    return thrdpool_destroy_internal(pool, pool->size);
}

#endif /* THRDPOOL_H */
