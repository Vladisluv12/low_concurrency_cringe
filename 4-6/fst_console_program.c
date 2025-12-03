#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include "flower.h"
#include <pthread.h>
#include <stdatomic.h>

// magic constants
#define NUM_PROCS 10
#define QUEUE_SIZE 10
#define SHM_NAME "/for-semaphor"
#define TIME_TO_WATER 2
#define TIME_WITHOUT_WATER 6 
// vars for ipc 
volatile atomic_int running = 1;
// types
// shared mem type for unnamed semaphore
typedef struct {
    int front, rear;
    int data[QUEUE_SIZE];
    sem_t mu;
    sem_t empty;
    sem_t full;
} queue_t;

typedef struct {
    struct Flower flowers[NUM_PROCS];
    sem_t sem[NUM_PROCS];
    queue_t queue;
} shared_mem_t;


shared_mem_t *buf;
int parent_pid;

void exit_handler(int signum) {
    if (getpid() != parent_pid) {
        printf("[CHILD] ");
    } else {
        printf("[PARENT] ");
    }
    printf("Proccess received SIGINT, shut down\n");
    running = 0;
}

int flower_proc(int id) {
    printf("flower process %d (pid %d) started\n", id, getpid());
    while (running) {
        sleep(buf->flowers[id].time_without_water);
        sem_wait(&buf->sem[id]);
        if (buf->flowers[id].st == WATERED) {
            buf->flowers[id].st = NEED_WATER;
            printf("flower %d needs water\n", id);
            
            if (!buf->flowers[id].in_queue) {
                sem_wait(&buf->queue.empty); // wait for the place in queue
                sem_wait(&buf->queue.mu); // wait if somebody else is writing/reading
                
                buf->queue.data[buf->queue.rear] = id;
                buf->queue.rear = (buf->queue.rear + 1) % QUEUE_SIZE;
                
                sem_post(&buf->queue.mu);
                sem_post(&buf->queue.full);
                
                buf->flowers[id].in_queue = 1; // put in queue
                printf("flower %d added to queue\n", id);
            }
        } else if (buf->flowers[id].st == NEED_WATER || buf->flowers[id].st == OVERWATERED) {
            buf->flowers[id].st = DEAD; // flower is dead (for real, no cap)
            if (buf->flowers[id].st == OVERWATERED) {
                printf("flower %d was overwatered and died\n", id);
            } else {
                printf("flower %d died\n", id);
            }
            sem_post(&buf->sem[id]);
            break;
        }
        sem_post(&buf->sem[id]);
    }
    printf("flower process %d exiting\n", id);
    return 0;
}

void* gardener(void* arg) {
    int gardener_id = *(int*)arg;
    free(arg);
    printf("gardener %d started\n", gardener_id);
    while (running) {
        sem_wait(&buf->queue.full);
        if (!running) break;
        sem_wait(&buf->queue.mu);
        int flower_id = buf->queue.data[buf->queue.front];
        buf->queue.front = (buf->queue.front + 1) % QUEUE_SIZE;
        sem_post(&buf->queue.mu);
        sem_post(&buf->queue.empty);

        sem_wait(&buf->sem[flower_id]);
        if (buf->flowers[flower_id].st == NEED_WATER) {
            if (rand() % 5 == 0) { // 20% chance of overwatering
                buf->flowers[flower_id].st = OVERWATERED;
            } else {
                buf->flowers[flower_id].st = WATERED;
            }
            buf->flowers[flower_id].in_queue = 0;
            printf("gardener %d watering flower %d\n", gardener_id, flower_id);
            sleep(buf->flowers[flower_id].time_to_water);
            printf("gardener %d finished watering flower %d\n", gardener_id, flower_id);
        } else {
            buf->flowers[flower_id].in_queue = 0;
        }

        sem_post(&buf->sem[flower_id]);
    }
    printf("gardener %d exiting\n", gardener_id);
    return NULL;
}

int main() {
    parent_pid = getpid();
    struct sigaction exit = {&exit_handler};
    if (sigaction(SIGINT, &exit, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    int shmfd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (shmfd < 0) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(shmfd, sizeof(shared_mem_t)) == -1) {
        perror("ftruncate");
        return 1;
    }
    buf = (shared_mem_t*)(mmap(NULL, sizeof(shared_mem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0));
    if (!buf) {
        perror("mmap");
        return 1;
    }
    for (int i = 0; i < NUM_PROCS; i++) {
        sem_init(&buf->sem[i], 1, 1);
        buf->flowers[i].id = i;
        buf->flowers[i].time_to_water = TIME_TO_WATER + rand() % 10;
        buf->flowers[i].time_without_water = TIME_WITHOUT_WATER + rand() % 7;
        buf->flowers[i].st = WATERED;
        buf->flowers[i].in_queue = 0;
    }

    buf->queue.front = 0;
    buf->queue.rear = 0;
    sem_init(&buf->queue.mu, 1, 1);
    sem_init(&buf->queue.empty, 1, QUEUE_SIZE);
    sem_init(&buf->queue.full, 1, 0);

    pid_t pids[NUM_PROCS];
    for (int i = 0; i < NUM_PROCS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            return flower_proc(i);
        } else if (pids[i] < 0) {
            perror("fork failed");
            return 1;
        }
    }
    
    pthread_t gardeners[2];
    for (int i = 0; i < 2; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&gardeners[i], NULL, gardener, id);
    }
    for (int i = 0; i < NUM_PROCS; i++) {
        waitpid(pids[i], NULL, 0);
    }
    running = 0;
    for (int i = 0; i < 2; i++) {
        sem_post(&buf->queue.full);
    }
    
    for (int i = 0; i < 2; i++) {
        pthread_join(gardeners[i], NULL);
    }
    
    for (int i = 0; i < NUM_PROCS; i++) {
        sem_destroy(&buf->sem[i]);
    }
    sem_destroy(&buf->queue.mu);
    sem_destroy(&buf->queue.empty);
    sem_destroy(&buf->queue.full);

    munmap(buf, sizeof(shared_mem_t));
    shm_unlink(SHM_NAME);
    printf("Main process finished\n");
}