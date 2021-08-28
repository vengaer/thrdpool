#ifndef TASK_H
#define TASK_H

typedef void(*thrdpool_taskhandle)(void *);

struct thrdpool_task {
    thrdpool_taskhandle handle;
    void *args;
};

inline void thrdpool_call(struct thrdpool_task const *task) {
    task->handle(task->args);
}

#endif /* TASK_H */
