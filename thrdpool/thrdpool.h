#ifndef THRDPOOL_H
#define THRDPOOL_H

#include "task.h"
#include "taskq.h"

#include <stdbool.h>
#include <stddef.h>

#include <pthread.h>

struct thrdpool {
    bool join;
    size_t size;
    size_t idle;
    pthread_cond_t cv;
    pthread_mutex_t lock;
    struct thrdpool_taskq q;
    pthread_t workers[];
};

#define thrdpool_bytesize(capacity) \
    (sizeof(struct thrdpool) + capacity * sizeof(((struct thrdpool *)0)->workers[0]))

#define thrdpool_decl(name, cap)                        \
    static union {                                      \
        unsigned char d_bytes[thrdpool_bytesize(cap)];  \
        struct thrdpool d_pool;                         \
    } name

#define thrdpool_init(u)                            \
    thrdpool_init_impl(&(u)->d_pool, (sizeof(*u) - sizeof((u)->d_pool)) / sizeof(pthread_t))

#define thrdpool_schedule(u, func, args)            \
    thrdpool_schedule_impl(&(u)->d_pool, func, args)

#define thrdpool_size(u)                            \
    (u)->d_pool.size

#define thrdpool_idle_workers(u)                    \
    thrdpool_idle_impl(&(u)->d_pool)

#define thrdpool_scheduled_tasks(u)                 \
    thrdpool_scheduled_tasks_impl(&(u)->d_pool)

#define thrdpool_destroy(u)                         \
    thrdpool_destroy_impl(&(u)->d_pool)

#define thrdpool_flush(u)                           \
    thrdpool_flush_impl(&(u)->d_pool)

#define thrdpool_taskq_capacity(u)                  \
    thrdpool_arrsize((u)->d_pool.q.tasks)

bool thrdpool_init_impl(struct thrdpool *pool, size_t capacity);

bool thrdpool_schedule_impl(struct thrdpool *pool, thrdpool_taskhandle task, void *args);

inline size_t thrdpool_idle_impl(struct thrdpool *pool) {
    size_t idle;
    pthread_mutex_lock(&pool->lock);
    idle = pool->idle;
    pthread_mutex_unlock(&pool->lock);
    return idle;
}

inline size_t thrdpool_scheduled_tasks_impl(struct thrdpool *pool) {
    size_t ntasks;
    pthread_mutex_lock(&pool->lock);
    ntasks = thrdpool_taskq_size(&pool->q);
    pthread_mutex_unlock(&pool->lock);
    return ntasks;
}

inline bool thrdpool_destroy_impl(struct thrdpool *pool) {
    extern bool thrdpool_destroy_internal(struct thrdpool *pool, size_t size);
    return thrdpool_destroy_internal(pool, pool->size);
}

inline void thrdpool_flush_impl(struct thrdpool *pool) {
    pthread_mutex_lock(&pool->lock);
    thrdpool_taskq_clear(&pool->q);
    pthread_mutex_unlock(&pool->lock);
}

#endif /* THRDPOOL_H */
