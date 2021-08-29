#include "shm.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) {
    if(!size) {
        return 0;
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

    shmb->size = size;
    memcpy(&shmb->data, data, size);

    kill(getppid(), SIGUSR1);
    sem_wait(&shmb->sem);

    munmap(shmb, sizeof(*shmb));

    return 0;
}
