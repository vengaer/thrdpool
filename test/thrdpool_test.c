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
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_inc, .args = &value }));

    pthread_cond_wait(&cv, &lock);

    TEST_ASSERT_EQUAL_UINT32(1u, value);
    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduling_procq_capacity(void) {
    struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_procq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));
    }

    /* Task queue full at this point */
    TEST_ASSERT_FALSE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));

    /* Wait for worker to finish */
    while(args.value < thrdpool_procq_capacity(&pool) + 1u) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(args.value, (unsigned)(thrdpool_procq_capacity(&pool) + 1u));

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduling_20_workers(void) {
    unsigned value = 0u;
    thrdpool_decl(pool, 20u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    unsigned max_enq = thrdpool_size(&pool) + thrdpool_procq_capacity(&pool);

    pthread_mutex_lock(&lock);

    for(unsigned i = 0u; i < max_enq; i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_inc, .args = &value }));
    }

    while(value < max_enq) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)thrdpool_scheduled_tasks(&pool));
    TEST_ASSERT_EQUAL_UINT32(max_enq, value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduling_128_workers(void) {
    unsigned value = 0u;
    thrdpool_decl(pool, 20u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    unsigned max_enq = thrdpool_size(&pool) + thrdpool_procq_capacity(&pool);

    pthread_mutex_lock(&lock);

    for(unsigned i = 0u; i < max_enq; i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_inc, .args = &value }));
    }

    while(value < max_enq) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)thrdpool_scheduled_tasks(&pool));
    TEST_ASSERT_EQUAL_UINT32(max_enq, value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_scheduled_tasks(void) {
    struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_procq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));
        TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), i + 1u);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), thrdpool_procq_capacity(&pool));

    /* Wait for worker to finish */
    while(args.value < thrdpool_procq_capacity(&pool) + 1u) {
        pthread_cond_wait(&cv, &lock);
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(args.value, (unsigned)(thrdpool_procq_capacity(&pool) + 1u));

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}

void test_schedule_flushing(void) {
    struct signalargs args;

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_init(&args.lock, 0), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_init(&args.cv, 0), 0);

    thrdpool_decl(pool, 1u);
    TEST_ASSERT_TRUE(thrdpool_init(&pool));

    pthread_mutex_lock(&lock);

    /* Schedule first task */
    pthread_mutex_lock(&args.lock);
    TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));

    /* Wait for worker to grab job from pool */
    pthread_cond_wait(&args.cv, &args.lock);
    /* Done with setup lock */
    pthread_mutex_unlock(&args.lock);

    /* Schedule enough jobs to fill up the queue while still holding
     * global lock */
    for(unsigned i = 0u; i < thrdpool_procq_capacity(&pool); i++) {
        TEST_ASSERT_TRUE(thrdpool_schedule(&pool, &(struct thrdpool_proc) { .handle = task_signal, .args = &args }));
    }

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), thrdpool_procq_capacity(&pool));

    thrdpool_flush(&pool);
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), 0u);

    /* Wait for worker to finish */
    pthread_cond_wait(&cv, &lock);

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_scheduled_tasks(&pool), 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, args.value);

    pthread_mutex_unlock(&lock);

    TEST_ASSERT_EQUAL_INT32(pthread_mutex_destroy(&args.lock), 0);
    TEST_ASSERT_EQUAL_INT32(pthread_cond_destroy(&args.cv), 0);
    TEST_ASSERT_TRUE(thrdpool_destroy(&pool));
}
