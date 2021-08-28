#include <unity.h>
#include <thrdpool/procq.h>

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

void test_procq_size(void) {
    struct thrdpool_procq q = thrdpool_procq_init();
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_procq_size(&q), 0u);

    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc }));
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_procq_size(&q), 1u);

    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc }));
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_procq_size(&q), 2u);

    thrdpool_procq_pop(&q);
    TEST_ASSERT_EQUAL_UINT32((unsigned)thrdpool_procq_size(&q), 1u);
}

void test_procq_call_extracted(void) {
    unsigned value = 0u;
    struct thrdpool_procq q = thrdpool_procq_init();
    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc, .args = &value }));

    struct thrdpool_proc *proc = thrdpool_procq_front(&q);
    TEST_ASSERT_EQUAL_UINT32(value, 0);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, gval);

    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc, .args = &value }));
    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = zero, .args = &value }));

    proc = thrdpool_procq_front(&q);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_procq_pop(&q);

    proc = thrdpool_procq_front(&q);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_procq_pop(&q);

    proc = thrdpool_procq_front(&q);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, 0);
    thrdpool_procq_pop(&q);
}

void test_procq_wrap(void) {
    unsigned value = 0u;

    struct thrdpool_procq q = thrdpool_procq_init();

    for(unsigned i = 0; i < thrdpool_arrsize(q.procs); i++) {
        TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc, .args = &value }));
    }
    TEST_ASSERT_FALSE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = zero, .args = &value }));
    thrdpool_procq_pop(&q);
    thrdpool_procq_pop(&q);

    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = zero, .args = &value }));
    TEST_ASSERT_TRUE(thrdpool_procq_push(&q, &(struct thrdpool_proc) { .handle = inc, .args = &value }));

    struct thrdpool_proc *proc;

    for(unsigned i = 0; i < thrdpool_arrsize(q.procs) - 2u; i++) {
        proc = thrdpool_procq_front(&q);
        thrdpool_call(proc);
        TEST_ASSERT_EQUAL_UINT32(value, gval);
        thrdpool_procq_pop(&q);
    }
    proc = thrdpool_procq_front(&q);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, 0u);
    thrdpool_procq_pop(&q);

    proc = thrdpool_procq_front(&q);
    thrdpool_call(proc);
    TEST_ASSERT_EQUAL_UINT32(value, gval);
    thrdpool_procq_pop(&q);
}
