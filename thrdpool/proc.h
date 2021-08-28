#ifndef PROC_H
#define PROC_H

typedef void(*thrdpool_prochandle)(void *);

struct thrdpool_proc {
    thrdpool_prochandle handle;
    void *args;
};

inline void thrdpool_call(struct thrdpool_proc const *proc) {
    proc->handle(proc->args);
}

#endif /* PROC_H */
