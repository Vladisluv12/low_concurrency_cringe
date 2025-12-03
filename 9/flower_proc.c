#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <mqueue.h>
#include <stdarg.h>
#include "structs.h"

mqd_t log_mq;

static shared_mem_t *shm = NULL;
static int shm_fd = -1;

static sem_t *queue_mutex = NULL;
static sem_t *queue_empty = NULL;
static sem_t *queue_full = NULL;
static sem_t *alive_sem = NULL;
static sem_t *flower_sem = NULL;

void log_msg(const char *fmt, ...) {
    if (log_mq == (mqd_t)-1) return;

    char msg[LOG_MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (mq_send(log_mq, msg, strlen(msg)+1, 0) == (mqd_t)-1) {
        log_mq = (mqd_t)-1;
    }
}

void cleanup() {
    sem_wait(alive_sem);
    shm->alive_flowers--;
    sem_post(alive_sem);
    if (queue_mutex) sem_close(queue_mutex);
    if (queue_empty) sem_close(queue_empty);
    if (queue_full)  sem_close(queue_full);
    if (alive_sem) sem_close(alive_sem);
    if (flower_sem) sem_close(flower_sem);

    if (shm && shm != MAP_FAILED) munmap(shm, sizeof(shared_mem_t));
    if (shm_fd != -1) close(shm_fd);
    if (log_mq != (mqd_t)-1) mq_close(log_mq);
}

void handle_sig(int sig) {
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Flower process must be started with ID arg.\n");
        log_msg("Flower process must be started with ID arg.\n");
        return 1;
    }

    int id = atoi(argv[1]);

    queue_mutex = sem_open(SEM_QUEUE_MUTEX, 0);
    queue_empty = sem_open(SEM_QUEUE_EMPTY, 0);
    queue_full  = sem_open(SEM_QUEUE_FULL, 0);
    alive_sem = sem_open(SEM_ALIVE, 0);

    char sem_name[32];
    sprintf(sem_name, "%s%d", SEM_FLOWER_BASE, id);
    flower_sem = sem_open(sem_name, 0);
    if (flower_sem == SEM_FAILED) {
        perror("sem_open flower");
        exit(1);
    }

    if (!queue_mutex || !queue_empty || !queue_full || !flower_sem) {
        perror("Failed to open semaphores");
        return 1;
    }

    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    shm = mmap(NULL, sizeof(shared_mem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    log_mq = mq_open(LOG_QUEUE, O_WRONLY | O_NONBLOCK);

    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    struct Flower *fl = &shm->flowers[id];
    fl->pid = getpid();
    fl->st = WATERED;
    fl->in_queue = 0;

    printf("[Flower #%d] Started. PID=%d\n", id, fl->pid);
    log_msg("[Flower #%d] Started. PID=%d\n", id, fl->pid);

    while (shm->system_running) {
        sleep(fl->time_without_water);
        sem_wait(flower_sem);
        if (fl->st == WATERED) {
            fl->st = NEED_WATER;
            fl->in_queue = 1;
            printf("[Flower #%d] needs water, pushing to queue\n", id);
            log_msg("[Flower #%d] needs water, pushing to queue\n", id);

            sem_wait(queue_empty);
            sem_wait(queue_mutex);

            shm->queue.data[shm->queue.rear] = id;
            shm->queue.rear = (shm->queue.rear + 1) % QUEUE_SIZE;
            shm->flowers[id].in_queue = 1;
            sem_post(queue_mutex);
            sem_post(queue_full);
        } else if (fl->st == NEED_WATER|| fl->st == OVERWATERED) {
            fl->st = (fl->st == OVERWATERED? OVERWATERED : DEAD);
            printf("[Flower #%d] died: %s\n", id, fl->st == OVERWATERED ? "OVERWATERED" : "DECAYED");
            log_msg("[Flower #%d] died: %s\n", id, fl->st == OVERWATERED ? "OVERWATERED" : "DECAYED");
            sem_post(flower_sem);
            break;
        }
        sem_post(flower_sem);
    }
    if (flower_sem) sem_close(flower_sem);
    cleanup();
}
