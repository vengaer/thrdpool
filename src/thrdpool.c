#include <thrdpool/thrdpool.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct thrdargs {
    bool *join;
    pthread_cond_t *cv;
    pthread_mutex_t *lock;
    struct thrdpool_procq *q;
    pthread_barrier_t *argbarrier;
};

bool thrdpool_destroy(struct thrdpool *pool);

static void *thrdpool_wait(void *p) {
    /* Copy args to stack */
    struct thrdargs args = *(struct thrdargs *)p;
    /* Release main thread */
    pthread_barrier_wait(args.argbarrier);

    struct thrdpool_proc proc;
    bool has_proc = false;
    bool join = false;

    while(!join) {
        has_proc = false;
        pthread_mutex_lock(args.lock);

        /* Avoid spurious wakeups */
        while(!*args.join && !thrdpool_procq_size(args.q)) {
            pthread_cond_wait(args.cv, args.lock);
        }

        join = *args.join;
        if(!join) {
            /* Copy first proc to stack */
            proc = *thrdpool_procq_front(args.q);
            thrdpool_procq_pop_front(args.q);
            has_proc = true;
        }

        pthread_mutex_unlock(args.lock);

        if(has_proc) {
            thrdpool_call(&proc);
        }
    }

    return 0;
}

bool thrdpool_destroy_internal(struct thrdpool *pool, size_t nthreads) {
    bool success = true;
    int err;
    /* Notify about pending join and wake up worker threads */
    pthread_mutex_lock(&pool->lock);
    pool->join = true;
    pthread_mutex_unlock(&pool->lock);

    err = pthread_cond_broadcast(&pool->cv);
    if(err) {
        fprintf(stderr, "Error unblocking threads for joining: %s\n", strerror(errno));
        /* Attempting to join would block indefinitely */
        return false;
    }

    for(size_t i = 0u; i < nthreads; i++) {
        err = pthread_join(pool->workers[i], 0);
        if(err) {
            fprintf(stderr, "Error joining worker %zu: %s\n", i, strerror(errno));
            success = false;
        }
    }

    err = pthread_mutex_destroy(&pool->lock);
    if(err) {
        fprintf(stderr, "Error destroying mutex: %s\n", strerror(errno));
        success = false;
    }
    err = pthread_cond_destroy(&pool->cv);
    if(err) {
        fprintf(stderr, "Error destroying condition variable: %s\n", strerror(errno));
        success = false;
    }

    return success;
}

struct thrdpool *thrdpool_init_impl(struct thrdpool *pool, size_t capacity) {
    int err;
    size_t nthreads;
    pthread_barrier_t argbarrier;

    struct thrdpool *outp = 0;

    pool->join = false;
    pool->q = thrdpool_procq_init();
    pool->size = capacity;

    err = pthread_cond_init(&pool->cv, 0);
    if(err) {
        fprintf(stderr, "Error intializing condition variable: %s\n", strerror(errno));
        return 0;
    }

    err = pthread_mutex_init(&pool->lock, 0);
    if(err) {
        fprintf(stderr, "Error initializing mutex: %s\n", strerror(errno));
        pthread_cond_destroy(&pool->cv);
        return 0;
    }

    err = pthread_barrier_init(&argbarrier, 0, 2u);
    if(err) {
        fprintf(stderr, "Error initializing barrier: %s\n", strerror(errno));
        goto epilogue;
    }

    for(; nthreads < pool->size; nthreads++) {
        struct thrdargs args = {
            .join = &pool->join,
            .cv = &pool->cv,
            .lock = &pool->lock,
            .q = &pool->q,
            .argbarrier = &argbarrier
        };
        err = pthread_create(&pool->workers[nthreads], 0, thrdpool_wait, &args);
        if(err) {
            fprintf(stderr, "Error forking thread %zu: %s\n", nthreads, strerror(errno));
            goto epilogue;
        }

        /* Wait for forked thread to copy args to its stack */
        pthread_barrier_wait(&argbarrier);
    }

    outp = pool;
epilogue:
    if(!outp) {
        thrdpool_destroy_internal(pool, nthreads);
    }
    err = pthread_barrier_destroy(&argbarrier);
    if(err) {
        fprintf(stderr, "Error destroying barrier: %s\n", strerror(errno));
    }
    return outp;
}

bool thrdpool_schedule(struct thrdpool *restrict pool, struct thrdpool_proc const *restrict proc) {
    bool success = true;

    pthread_mutex_lock(&pool->lock);
    success = thrdpool_procq_push(&pool->q, proc);
    pthread_mutex_unlock(&pool->lock);

    if(!success) {
        return false;
    }

    pthread_cond_signal(&pool->cv);
    return true;
}
