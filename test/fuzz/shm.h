#ifndef SHM_H
#define SHM_H

#include <stddef.h>
#include <stdint.h>

#include <semaphore.h>

#define SHMPATH "/thrdpoolfuzz"

struct shmbuf {
    sem_t sem;
    size_t size;
    uint8_t data[FUZZ_MAXLEN];
};

#endif /* SHM_H */
