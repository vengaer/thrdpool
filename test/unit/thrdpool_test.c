#include <unity.h>

#include <thrdpool/thrdpool.h>

#include <pthread.h>

static pthread_mutex_t lock;
static pthread_cond_t cv;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&cv, 0), 0);
}

void tearDown(void) {
    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&cv), 0);
}

struct signalargs {
    pthread_mutex_t lock;
    pthread_cond_t cv;
    unsigned value;
};

void task_inc(void *arg) {
    pthread_mutex_lock(&lock);
    ++(*(unsigned *)arg);
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&cv);
}

void task_signal(void *args) {
    struct signalargs *sa = args;

    /* Wait for main thread to release lock */
    pthread_mutex_lock(&sa->lock);
    pthread_mutex_unlock(&sa->lock);
    /* Release main thread */
    pthread_cond_signal(&sa->cv);

    pthread_mutex_lock(&lock);
    ++sa->value;
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&cv);
}

void test_thrdpool_size(void) {
    {
        thrdpool_decl(pool, 1u);
        TEST_ASSERT_EQUAL_UINT32((unsigned)sizeof(pool), sizeof(struct thrdpool) + sizeof(pthread_t));
        TEST_ASSERT_TRUE(thrdpool_init(&pool));
        TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_size(&pool), 1u);
        TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
    }
    {
        thrdpool_decl(pool, 8u);
        TEST_ASSERT_EQUAL_UINT32((unsigned)sizeof(pool), sizeof(struct thrdpool) + 8u * sizeof(pthread_t));
        TEST_ASSERT_TRUE(thrdpool_init(&pool));
        TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_size(&pool), 8u);
        TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
    }
}

void test_basic_scheduling(void) {
    unsigned value = 0u;
    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_inc, &value));

    pthread_cond_wait(&cv, &lock);

    TEST_ASSERT_EQUAL_UINT32(1u, value);
    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduling_taskq_capacity(void) {
    static struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_taskq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));
    }

    /* Task queue full at this point */
    TEST_ASSERT_FALSE(thrdpool_schedule(&pool, task_signal, &args));

    /* Wait for worker to finish */
    while(args.value < thrdpool_taskq_capacity(&pool) + 1u) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(args.value, (unsigned)(thrdpool_taskq_capacity(&pool) + 1u));

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
}

void test_scheduling_20_workers(void) {
    unsigned value = 0u;
    thrdpool_decl(pool, 20u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    unsigned max_enq = thrdpool_size(&pool) + thrdpool_taskq_capacity(&pool);

    pthread_mutex_lock(&lock);

    for(unsigned i = 0u; i < max_enq; i++) {
        /* If queue is full, allow workers some time to pick tasks */
        while(!thrdpool_schedule(&pool, task_inc, &value)) {
            pthread_yield();
        }
    }

    while(value < max_enq) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)thrdpool_pending(&pool));
    TEST_ASSERT_EQUAL_UINT32(max_enq, value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduling_128_workers(void) {
    unsigned value = 0u;
    thrdpool_decl(pool, 20u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    unsigned max_enq = thrdpool_size(&pool) + thrdpool_taskq_capacity(&pool);

    pthread_mutex_lock(&lock);

    for(unsigned i = 0u; i < max_enq; i++) {
        /* If queue is full, allow workers time to pick tasks */
        while(!thrdpool_schedule(&pool, task_inc, &value)) {
            pthread_yield();
        }
    }

    while(value < max_enq) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)thrdpool_pending(&pool));
    TEST_ASSERT_EQUAL_UINT32(max_enq, value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_pending(void) {
    static struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_taskq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));
        TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), i + 1u);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), thrdpool_taskq_capacity(&pool));

    /* Wait for worker to finish */
    while(args.value < thrdpool_taskq_capacity(&pool) + 1u) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(args.value, (unsigned)(thrdpool_taskq_capacity(&pool) + 1u));

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
}

void test_schedule_flushing(void) {
    static struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_taskq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, task_signal, &args));
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), thrdpool_taskq_capacity(&pool));

    thrdpool_flush(&pool);
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), 0u);

    /* Wait for worker to finish */
    pthread_cond_wait(&cv, &lock);

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_pending(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, args.value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
}
