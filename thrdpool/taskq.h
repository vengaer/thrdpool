#ifndef TASKQ_H
#define TASKQ_H

#include "task.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef THRDPOOL_TASKQ_CAPACITY
#define THRDPOOL_TASKQ_CAPACITY 32u
#endif

#define thrdpool_is_power_of_2(x)    \
    (((x) & -(x)) == (x))

#if 0xffffffff == -1 && thrdpool_is_power_of_2(THRDPOOL_TASKQ_CAPACITY)
#define thrdpool_mod_size(x) ((x) & THRDPOOL_TASKQ_CAPACITY)
#else
#define thrdpool_mod_size(x) ((x) % THRDPOOL_TASKQ_CAPACITY)
#endif


#define thrdpool_arrsize(x) (sizeof(x) / sizeof((x)[0]))

struct thrdpool_taskq {
    size_t start;
    size_t size;
    struct thrdpool_task tasks[THRDPOOL_TASKQ_CAPACITY];
};

#define thrdpool_taskq_init() (struct thrdpool_taskq) { .start = 0u, .size = 0u }

bool thrdpool_taskq_push(struct thrdpool_taskq *restrict q, thrdpool_taskhandle task, void *args);
struct thrdpool_task *thrdpool_taskq_front(struct thrdpool_taskq *q);

inline void thrdpool_taskq_pop_front(struct thrdpool_taskq *q) {
    assert(q->size);
    --q->size;
    q->start = thrdpool_mod_size(q->start + 1u);
}

inline void thrdpool_taskq_pop_back(struct thrdpool_taskq *q) {
    assert(q->size);
    --q->size;
}


inline size_t thrdpool_taskq_size(struct thrdpool_taskq const *q) {
    return q->size;
}

inline void thrdpool_taskq_clear(struct thrdpool_taskq *q) {
    q->size = 0u;
}

#endif /* TASKQ_H */
