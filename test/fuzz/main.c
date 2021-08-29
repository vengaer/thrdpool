#include "shm.h"

#include <thrdpool/thrdpool.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define str(x) #x
#define str_expand(x) str(x)

static pthread_mutex_t lock;
static pthread_cond_t cv;
static unsigned processed;
static unsigned parsum;

static unsigned volatile child_alive = 1;
static unsigned volatile alive = 1;

void sighandler(int sig) {
    switch(sig) {
        case SIGCHLD:
            child_alive = 0;
            alive = 0;
            break;
        case SIGINT:
            alive = 0;
            break;
        default:
            /* nop */
            break;
    }
}

bool set_sighandler(void) {
    struct sigaction sa = { 0 };
    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("sigaction");
        return false;
    }
    if(sigaction(SIGINT, &sa, 0) == -1) {
        perror("sigaction");
        return false;
    }
    return true;
}

void accumulate(void *p) {
    pthread_mutex_lock(&lock);
    parsum += *(uint8_t *)p;
    ++processed;
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&cv);
}

thrdpool_decl(pool, 32);

bool process(struct shmbuf *shmb) {
    unsigned seqsum = 0u;
    bool success = true;

    static uint8_t data[FUZZ_MAXLEN];
    size_t size = shmb->size;
    memcpy(data, shmb->data, size);

    if(!size) {
        return true;
    }

    pthread_mutex_lock(&lock);
    parsum = 0u;
    processed = 0u;
    pthread_mutex_unlock(&lock);

    for(unsigned i = 0; i < size; i++) {
        seqsum += data[i];
        while(!thrdpool_schedule(&pool, &(struct thrdpool_task) { .handle = accumulate, &data[i] })) {
            pthread_yield();
        }
    }

    /* Wait until all data has been processed */
    pthread_mutex_lock(&lock);
    while(processed < size) {
        pthread_cond_wait(&cv, &lock);
    }

    if(seqsum != parsum) {
        fprintf(stderr, "Sums do not match, sequential: %u, parallel: %u\n", seqsum, parsum);
        success = false;
    }
    pthread_mutex_unlock(&lock);

    return success;
}

int main(int argc, char **argv) {
    (void) argc;
    int status = 1;
    int childstatus;
    int err;
    pid_t childpid = 0;
    bool sem_inited = false;
    bool thrdpool_inited = false;
    struct shmbuf *shmb = MAP_FAILED;

    err = pthread_cond_init(&cv, 0);
    if(err) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(err));
        return 1;
    }
    err = pthread_mutex_init(&lock, 0);
    if(err) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(err));
        pthread_cond_destroy(&cv);
        return 1;
    }

    int fd = shm_open(SHMPATH, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(fd == -1) {
        perror("shm_open");
        pthread_cond_destroy(&cv);
        pthread_mutex_destroy(&lock);
        return 1;
    }

    if(ftruncate(fd, sizeof(*shmb))) {
        perror("ftruncate");
        goto epilogue;
    }

    shmb = mmap(0, sizeof(*shmb), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shmb == MAP_FAILED) {
        perror("mmap");
        goto epilogue;
    }

    if(sem_init(&shmb->sem, 1, 1)) {
        perror("sem_init");
        goto epilogue;
    }
    sem_inited = true;

    if(!set_sighandler()) {
        goto epilogue;
    }

    childpid = fork();
    switch(childpid) {
        case -1:
            perror("fork");
            goto epilogue;
        case 0:
            munmap(shmb, sizeof(*shmb));
            close(fd);
            execv(str_expand(FUZZER_GENPATH), argv);
        default:
            /* nop */
            break;
    }

    printf("Forked generator with pid %lld\n", (long long)childpid);

    if(!thrdpool_init(&pool)) {
        fputs("thrdpool_init\n", stderr);
        goto epilogue;
    }
    thrdpool_inited = true;

    while(alive) {
        err = waitpid(childpid, &childstatus, WNOHANG);
        switch(err) {
            case -1:
                perror("waitpid");
                goto epilogue;
            case 0:
                sem_wait(&shmb->sem);
                err = !process(shmb);
                if(sem_post(&shmb->sem) == -1) {
                    perror("sem_post");
                    goto epilogue;
                }
                if(err) {
                    goto epilogue;
                }
                break;
            default:
                child_alive = 0;
                alive = 0;
                break;
         }
    }

    status = WIFEXITED(childstatus) ? WEXITSTATUS(childstatus) : 1;
epilogue:
    if(child_alive) {
        kill(childpid, SIGTERM);
    }
    if(thrdpool_inited) {
        thrdpool_destroy(&pool);
    }
    if(sem_inited) {
        sem_destroy(&shmb->sem);
    }
    if(shmb != MAP_FAILED) {
        munmap(shmb, sizeof(*shmb));
    }
    close(fd);
    shm_unlink(SHMPATH);
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    return status;
}
