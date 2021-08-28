#include <thrdpool/thrdpool.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

size_t thrdpool_scheduled_tasks_impl(struct thrdpool *pool);
bool thrdpool_destroy_impl(struct thrdpool *pool);
void thrdpool_flush_impl(struct thrdpool *pool);

static void *thrdpool_wait(void *p) {
    struct thrdpool *pool = p;

    struct thrdpool_proc proc;
    bool has_proc = false;
    bool join = false;

    while(!join) {
        has_proc = false;
        pthread_mutex_lock(&pool->lock);

        /* Avoid spurious wakeups */
        while(!pool->join && !thrdpool_procq_size(&pool->q)) {
            pthread_cond_wait(&pool->cv, &pool->lock);
        }

        join = pool->join;
        if(!join) {
            /* Copy first proc to stack */
            proc = *thrdpool_procq_front(&pool->q);
            thrdpool_procq_pop_front(&pool->q);
            has_proc = true;
        }

        pthread_mutex_unlock(&pool->lock);

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

bool thrdpool_init_impl(struct thrdpool *pool, size_t capacity) {
    int err;
    size_t nthreads = 0u;
    bool success = false;

    if(!capacity) {
        return false;
    }

    pool->join = false;
    pool->q = thrdpool_procq_init();
    pool->size = capacity;

    err = pthread_cond_init(&pool->cv, 0);
    if(err) {
        fprintf(stderr, "Error intializing condition variable: %s\n", strerror(errno));
        return false;
    }

    err = pthread_mutex_init(&pool->lock, 0);
    if(err) {
        fprintf(stderr, "Error initializing mutex: %s\n", strerror(errno));
        pthread_cond_destroy(&pool->cv);
        return false;
    }

    for(; nthreads < pool->size; nthreads++) {
        err = pthread_create(&pool->workers[nthreads], 0, thrdpool_wait, pool);
        if(err) {
            fprintf(stderr, "Error forking thread %zu: %s\n", nthreads, strerror(errno));
            goto epilogue;
        }
    }

    success = true;
epilogue:
    if(!success) {
        thrdpool_destroy_internal(pool, nthreads);
    }
    return success;
}

bool thrdpool_schedule_impl(struct thrdpool *restrict pool, struct thrdpool_proc const *restrict proc) {
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
