#include <unity.h>
#include <thrdpool/taskq.h>

static unsigned gval = 32;

void inc(void *args) {
    if(args) {
        *(unsigned *)args = ++gval;
    }
}

void zero(void *args) {
    if(args) {
        *(unsigned *)args = 0u;
    }
}

void test_taskq_size(void) {
    struct thrdpool_taskq q = thrdpool_taskq_init();
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 0u);

    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc }));
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 1u);

    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc }));
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 2u);

    thrdpool_taskq_pop_front(&q);
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 1u);
}

void test_taskq_call_extracted(void) {
    unsigned value = 0u;
    struct thrdpool_taskq q = thrdpool_taskq_init();
    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc, .args = &value }));

    struct thrdpool_task *task = thrdpool_taskq_front(&q);
    TEST_ASSERT_EQUAL_UINT32(value, 0);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, gval);

    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc, .args = &value }));
    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = zero, .args = &value }));

    task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_taskq_pop_front(&q);

    task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_taskq_pop_front(&q);

    task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, 0);
    thrdpool_taskq_pop_front(&q);
}

void test_taskq_wrap(void) {
    unsigned value = 0u;

    struct thrdpool_taskq q = thrdpool_taskq_init();

    for(unsigned i = 0; i < thrdpool_arrsize(q.tasks); i++) {
        TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc, .args = &value }));
    }
    TEST_ASSERT_FALSE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = zero, .args = &value }));
    thrdpool_taskq_pop_front(&q);
    thrdpool_taskq_pop_front(&q);

    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = zero, .args = &value }));
    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc, .args = &value }));

    struct thrdpool_task *task;

    for(unsigned i = 0; i < thrdpool_arrsize(q.tasks) - 2u; i++) {
        task = thrdpool_taskq_front(&q);
        thrdpool_call(task);
        TEST_ASSERT_EQUAL_UINT32(value, gval);
        thrdpool_taskq_pop_front(&q);
    }
    task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, 0u);
    thrdpool_taskq_pop_front(&q);

    task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_taskq_pop_front(&q);
}

void test_taskq_pop_back(void) {
    unsigned value = 0u;

    struct thrdpool_taskq q = thrdpool_taskq_init();

    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = inc, .args = &value }));
    TEST_ASSERT_TRUE(thrdpool_taskq_push(&q, &(struct thrdpool_task) { .handle = zero, .args = &value }));

    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 2u);
    thrdpool_taskq_pop_back(&q);
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_taskq_size(&q), 1u);
    struct thrdpool_task *task = thrdpool_taskq_front(&q);
    thrdpool_call(task);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
}
