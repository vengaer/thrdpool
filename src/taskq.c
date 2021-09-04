#include <thrdpool/taskq.h>

void thrdpool_taskq_pop_front(struct thrdpool_taskq *q);
void thrdpool_taskq_pop_back(struct thrdpool_taskq *q);
size_t thrdpool_taskq_size(struct thrdpool_taskq const *q);
void thrdpool_taskq_clear(struct thrdpool_taskq *q);

bool thrdpool_taskq_push(struct thrdpool_taskq *q, thrdpool_taskhandle task, void *args) {
    if(q->size == thrdpool_arrsize(q->tasks)) {
        return false;
    }
    q->tasks[thrdpool_mod_size(q->start + q->size)] = (struct thrdpool_task) {
        .handle = task,
        .args = args
    };
    ++q->size;
    assert(q->size <= thrdpool_arrsize(q->tasks));
    return true;
}

struct thrdpool_task *thrdpool_taskq_front(struct thrdpool_taskq *q) {
    if(!q->size) {
        return 0;
    }
    assert(q->start < thrdpool_arrsize(q->tasks));
    return &q->tasks[q->start];
}
