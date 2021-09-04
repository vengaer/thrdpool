thrdpool - A Thread Pool Library

Provides simple scheduling of tasks across statically allocated thread pools. Requires a POSIX-compliant C compiler.

It should goes saying but while the thread pool endeavors to be internally thread safe, shared data used in the
tasks executed must be synchronized separately.

[[_TOC_]]

## Example Usage

The typical workflow can be summarized in 4 steps

1. Create a thread pool using the `thrdpool_decl` macro.
2. Initialize the pool via `thrdpool_init`.
3. Schedule tasks using `thrdpool_schedule`.
4. Once done, release the resources using `thrdpool_destroy`.

The following is a simple example of the above
```c
#include <thrdpool/thrdpool.h>
#include <pthread.h>

void triple(void *i) {
    *(unsigned *)i *= 3u;
}

int main(void) {
    /* Array of values to triple */
    unsigned values[40];
    for(unsigned i = 0u; i < 40u; i++) {
        values[i] = i + 1;
    }

    /* Thread pool p with 40 threads */
    thrdpool_decl(p, 40u);

    if(!thrdpool_init(&p)) {
        return 1;
    }

    for(unsigned i = 0; i < 40u; i++) {
        while(!thrdpool_schedule(&p, triple, &values[i])) {
            /* Queue is full, yield */
            pthread_yield();
        }
    }

    /* Join threads and destroy synchronization primitives */
    if(!thrdpool_destroy(&p)) {
        return 1;
    }

    for(unsigned i = 0u; i < 40u; i++) {
        assert(values[i] == (i + 1) * 3u);
    }

    return 0;
}
```

## Tasks

Any function with the signature `void name(void *)` may be scheduled for execution. Scheduled tasks are
appended to a FIFO task structure in the thread pool. Once a worker thread finishes a task, it pops the
next from the queue. If no tasks are available, the worker goes to sleep until one is scheduled.

The size of the task queue may be read using `thrdpool_pending`, whereas the max capacity is
given by `thrdpool_taskq_capacity`. Clearing the task queue is done by calling `thrdpool_flush`.

## Library Reference

As no two thread pools have the same type (although some may be identical byte for byte), this
section refers to the type of a pool created by `thrdpool_decl` as `/* pooltype */`.

#### `thrdpool_decl(name, nthreads)`

Declares a thread pool `name` with `nthreads` threads. The structure has static storage duration.

#### `bool thrdpool_init(/* pooltype */ *pool)`

Initializes the thread pool at address `pool`. 

Returns: `true` if the initialization succeeded. Failures are caused by error during initialization of 
         synchronization primitives or while spawning threads.

#### `bool thrdpool_destroy(/* pooltype */ *pool)`

Destroy the thread pool at address `pool`. Joins the worker threads, waiting for non-idle ones.  Vacant 
tasks in the task queue, if any, are ignored.

Returns: `true` if workers could be joined and synchronization primitives destroyed.

#### `bool thrdpool_schedule(/* pooltype */ * pool, void(*task)(void *), void *args)`

Add a task to `pool`'s task queue. The `args` parameter is passed on to `task` when invoked, meaning
the data must be both accessible and valid for the entire lifetime of the task.

The success of the operation depends on the number of tasks currently scheduled. The default capacity 
of the task queue is 32 but can be overridden by defining the `THRDPOOL_TASKQ_CAPACITY` before the
header is included.

Returns: `true` is the task could be pushed to the queue.

#### `size_t thrdpool_size(/* pooltype */ *pool)`

Returns: The total number of worker threads in the pool.

#### `size_t thrdpool_pending(/* pooltype */ *pool)`

Returns: The number of tasks in the queue (i.e. tasks that are yet to be executed)

#### `void thrdpool_flush(/* pooltype */ *pool)`

Flushes the task queue of the pool.

#### `size_t thrdpool_taskq_capacity(/* pooltype */ *pool)`

Returns: The max number of tasks the task queue of `pool` can hold. The number is determined by `THRDPOOL_TASKQ_CAPACITY` (see above).
