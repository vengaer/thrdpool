#include <thrdpool/procq.h>

void thrdpool_procq_pop(struct thrdpool_procq *q);
size_t thrdpool_procq_size(struct thrdpool_procq const *q);

bool thrdpool_procq_push(struct thrdpool_procq *restrict q, struct thrdpool_proc const *restrict handle) {
    if(q->size == thrdpool_arrsize(q->procs)) {
        return false;
    }
    q->procs[thrdpool_mod_size(q->start + q->size)] = *handle;
    ++q->size;
    return true;
}

struct thrdpool_proc *thrdpool_procq_top(struct thrdpool_procq *q) {
    if(!q->size) {
        return 0;
    }
    return &q->procs[q->start];
}
