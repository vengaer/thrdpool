#include "shm.h"

#include <thrdpool/thrdpool.h>

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

void sighandler(int signal) {
    (void)signal;
}

static bool set_sighandler(void) {
    bool success = true;
    struct sigaction sa = { 0 };
    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGCHLD, &sa, 0)) {
        perror("sigaction SIGCHLD");
        success = false;
    }
    if(sigaction(SIGUSR1, &sa, 0)) {
        perror("sigaction SIGUSR1");
        success = false;
    }
    return success;
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

    if(!shmb->size) {
        return true;
    }

    pthread_mutex_lock(&lock);
    parsum = 0u;
    processed = 0u;

    for(unsigned i = 0; i < shmb->size; i++) {
        while(!thrdpool_schedule(&pool, &(struct thrdpool_task) { .handle = accumulate, &shmb->data[i] })) {
            pthread_yield();
        }
    }

    /* Wait until all data has been processed */
    while(processed < shmb->size) {
        pthread_cond_wait(&cv, &lock);
    }

    pthread_mutex_unlock(&lock);

    for(unsigned i = 0; i < shmb->size; i++) {
        seqsum += shmb->data[i];
    }

    if(seqsum != parsum) {
        fprintf(stderr, "Sums do not match, sequential: %u, parallel: %u\n", seqsum, parsum);
        success = false;
    }

    if(sem_post(&shmb->sem) == -1) {
        perror("sem_post");
        return false;
    }
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

    puts("Forking child");
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

    if(!thrdpool_init(&pool)) {
        fputs("thrdpool_init\n", stderr);
        goto epilogue;
    }
    thrdpool_inited = true;

    while(1) {
        err = wait(&childstatus);
        if(err != -1) {
            break;
        }
        /* Interrupted by signal ? */
        if(errno == EINTR) {
            if(!process(shmb)) {
                goto epilogue;
            }
        }
        else {
            perror("wait");
            goto epilogue;
        }
    }

    childpid = 0;
    status = WIFEXITED(childstatus) ? WEXITSTATUS(childstatus) : 1;
epilogue:
    if(childpid) {
        kill(childpid, SIGABRT);
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
