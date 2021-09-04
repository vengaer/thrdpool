#include "shm.h"

#include <thrdpool/thrdpool.h>

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned processed;
static unsigned parsum;

static void accumulate(void *p) {
    parsum += *(uint8_t *)p;
    ++processed;
}

thrdpool_decl(pool, 1);

static bool process(uint8_t const *data, size_t size) {
    unsigned seqsum = 0u;
    bool success = true;

    if(!size) {
        return true;
    }

    parsum = 0u;
    processed = 0u;

    for(unsigned i = 0; i < size; i++) {
        seqsum += data[i];
        while(!thrdpool_schedule(&pool, accumulate, (void *)&data[i])) {
            pthread_yield();
        }
    }

    while(processed < size) {
        pthread_yield();
    }

    if(seqsum != parsum) {
        fprintf(stderr, "Sums do not match, sequential: %u, parallel: %u\n", seqsum, parsum);
        success = false;
    }

    return success;
}

static void cleanup(void) {
    thrdpool_destroy(&pool);
}

int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) {
    if(!size) {
        return 0;
    }
    static bool initialized = false;
    if(!initialized) {
        assert(thrdpool_init(&pool));
        atexit(cleanup);
        initialized = true;
    }
    assert(size <= FUZZ_MAXLEN);
    int fd = shm_open(SHMPATH, O_RDWR, S_IRUSR | S_IWUSR);
    if(fd == -1) {
        perror("shm_open");
        abort();
    }

    struct shmbuf *shmb = mmap(0, sizeof(*shmb), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if(shmb == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    sem_wait(&shmb->sem);

    shmb->size = size;
    memcpy(&shmb->data, data, size);

    if(sem_post(&shmb->sem) == -1) {
        perror("sem_post");
        abort();
    }

    munmap(shmb, sizeof(*shmb));

    process(data, size);

    return 0;
}
