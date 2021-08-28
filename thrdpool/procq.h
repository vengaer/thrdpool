#ifndef PROCQ_H
#define PROCQ_H

#include "proc.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef THRDPOOL_PROCQ_CAPACITY
#define THRDPOOL_PROCQ_CAPACITY 32u
#endif

#define thrdpool_is_power_of_2(x)    \
    (((x) & -(x)) == (x))

#if 0xffffffff == -1 && thrdpool_is_power_of_2(THRDPOOL_PROCQ_CAPACITY)
#define thrdpool_mod_size(x) ((x) & THRDPOOL_PROCQ_CAPACITY)
#else
#define thrdpool_mod_size(x) ((x) % THRDPOOL_PROCQ_CAPACITY)
#endif


#define thrdpool_arrsize(x) (sizeof(x) / sizeof((x)[0]))

struct thrdpool_procq {
    size_t start;
    size_t size;
    struct thrdpool_proc procs[THRDPOOL_PROCQ_CAPACITY];
};

#define thrdpool_procq_init() (struct thrdpool_procq) { .start = 0u, .size = 0u }

bool thrdpool_procq_push(struct thrdpool_procq *restrict q, struct thrdpool_proc const *restrict handle);
struct thrdpool_proc *thrdpool_procq_front(struct thrdpool_procq *q);

inline void thrdpool_procq_pop_front(struct thrdpool_procq *q) {
    assert(q->size);
    --q->size;
    q->start = thrdpool_mod_size(q->start + 1u);
}

inline void thrdpool_procq_pop_back(struct thrdpool_procq *q) {
    assert(q->size);
    --q->size;
}


inline size_t thrdpool_procq_size(struct thrdpool_procq const *q) {
    return q->size;
}

inline void thrdpool_procq_clear(struct thrdpool_procq *q) {
    q->size = 0u;
}

#endif /* PROCQ_H */
