#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <semaphore.h>
#include "structs.h"

static shared_mem_t *shm = NULL;
static int shm_fd = -1;

static sem_t *queue_mutex = NULL;
static sem_t *queue_empty = NULL;
static sem_t *queue_full  = NULL;
static sem_t *alive_sem = NULL;
static sem_t *flower_sem[NUM_PROCS];

pid_t flower_pids[NUM_PROCS];

int queue_pop(shared_mem_t *sh) {
    int id = sh->queue.data[sh->queue.front];
    sh->queue.front = (sh->queue.front + 1) % QUEUE_SIZE;
    return id;
}

void *gardener_thread(void *arg) {
    long gardener_id = (long)arg;

    printf("[Gardener %ld] started\n", gardener_id);

    while (shm && shm->system_running) {
        sem_wait(queue_full);

        if (!shm->system_running) break;
        sem_wait(queue_mutex);

        int id = queue_pop(shm);
		sem_wait(flower_sem[id]);
        struct Flower *fl = &shm->flowers[id];

        fl->in_queue = 0;
		
        if (shm->flowers[id].st == NEED_WATER) {
            if (rand() % 5 == 0) { // 20% chance of overwatering
                shm->flowers[id].st = OVERWATERED;
            } else {
                shm->flowers[id].st = WATERED;
            }
            shm->flowers[id].in_queue = 0;
            printf("[gardener %ld] watering flower %d\n", gardener_id, id);
            sleep(shm->flowers[id].time_to_water);
            printf("[gardener %ld] finished watering flower %d\n", gardener_id, id);
        } else {
            shm->flowers[id].in_queue = 0;
        }

        sem_post(queue_mutex);
        sem_post(queue_empty);
		sem_post(flower_sem[id]);
    }
    if (queue_full) sem_post(queue_full);
    printf("[Gardener %ld] exiting\n", gardener_id);
    return NULL;
}

void cleanup() {
    printf("[MAIN] Cleanup started\n");

    for (int i = 0; i < NUM_PROCS; i++) {
        if (flower_pids[i] > 0) {
			if (flower_sem[i]) sem_close(flower_sem[i]);
            if (kill(flower_pids[i], 0) == 0) kill(flower_pids[i], SIGTERM);
        }
    }
    for (int i = 0; i < NUM_PROCS; i++) {
        waitpid(flower_pids[i], NULL, 0);
    }

    if (queue_mutex) sem_close(queue_mutex);
    if (queue_empty) sem_close(queue_empty);
    if (queue_full)  sem_close(queue_full);
	if (alive_sem) sem_close(alive_sem);

    sem_unlink(SEM_QUEUE_MUTEX);
    sem_unlink(SEM_QUEUE_EMPTY);
    sem_unlink(SEM_QUEUE_FULL);
	sem_unlink(SEM_ALIVE);
	for (int i = 0; i < NUM_PROCS; i++) {
		char sem_name[32];
		sprintf(sem_name, "%s%d", SEM_FLOWER_BASE, i);
		sem_unlink(sem_name);
	}

    if (shm && shm != MAP_FAILED) munmap(shm, sizeof(shared_mem_t));
    if (shm_fd != -1) close(shm_fd);

    shm_unlink(SHM_NAME);

    printf("[MAIN] Cleanup completed\n");
    exit(0);
}

void sig_handler(int sig) {
    shm->system_running = 0;
    cleanup();
    exit(0);
}

int main() {
    signal(SIGINT, sig_handler);

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_mem_t));

    shm = mmap(NULL, sizeof(shared_mem_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    memset(shm, 0, sizeof(shared_mem_t));
    shm->system_running = 1;

    sem_unlink(SEM_QUEUE_MUTEX);
    sem_unlink(SEM_QUEUE_EMPTY);
    sem_unlink(SEM_QUEUE_FULL);
	sem_unlink(SEM_ALIVE);

    queue_mutex = sem_open(SEM_QUEUE_MUTEX, O_CREAT, 0666, 1);
    queue_empty = sem_open(SEM_QUEUE_EMPTY, O_CREAT, 0666, QUEUE_SIZE);
    queue_full  = sem_open(SEM_QUEUE_FULL,  O_CREAT, 0666, 0);
	alive_sem = sem_open(SEM_ALIVE, O_CREAT, 0666, 1);
	
    for (int i = 0; i < NUM_PROCS; i++) {
		char sem_name[32];
		sprintf(sem_name, "%s%d", SEM_FLOWER_BASE, i);
		flower_sem[i] = sem_open(sem_name, O_CREAT, 0666, 1);
		if (flower_sem[i] == SEM_FAILED) {
			perror("sem_open flower");
			exit(1);
		}
        shm->flowers[i].id = i;
        shm->flowers[i].st = WATERED;
        shm->flowers[i].time_without_water = TIME_WITHOUT_WATER + rand() % 2;
        shm->flowers[i].time_to_water = TIME_TO_WATER + rand() % 3;
		shm->flowers[i].in_queue = 0;
        shm->flowers[i].pid = 0; 
    }

	shm->alive_flowers = NUM_PROCS;
	shm->queue.front = 0;
	shm->queue.rear = 0;

    // for (int i = 0; i < NUM_PROCS; i++) {
    //     pid_t pid = fork();
    //     if (pid == 0) {
    //         char idbuf[10];
    //         sprintf(idbuf, "%d", i);
    //         execl("./flower_proc", "flower_proc", idbuf, NULL);
    //         perror("execl");
    //         exit(1);
    //     }
    // }

	printf("Ожидаю запуска всех %d процессов-цветов...\n", NUM_PROCS);

	while (1) {
		int count = 0;
		for (int i = 0; i < NUM_PROCS; i++) {
			if (shm->flowers[i].pid > 0)
				count++;
		}
		if (count == NUM_PROCS) {
			printf("Все %d процессов запущены!\n", NUM_PROCS);
			break;
		}
		printf("Запущено %d/%d процессов. Ожидаю...\n", count, NUM_PROCS);
		sleep(1);
	}

	for (int i = 0; i < NUM_PROCS; i++) {
		flower_pids[i] = shm->flowers[i].pid;
	}

    pthread_t gardeners[NUM_THRS];
    for (long i = 0; i < NUM_THRS; i++) {
        pthread_create(&gardeners[i], NULL, gardener_thread, (void *)i);
    }

    printf("[MAIN] System started. Press Ctrl+C to stop or wait, when all flowers will die.\n");

    while (shm->system_running) {
		printf("[MAIN] this much flowers are alive now: %d\n", shm->alive_flowers);
		if (shm->alive_flowers <= 0) {
			shm->system_running = 0;
            for (int i = 0; i < NUM_THRS; i++) {
                sem_post(queue_full);
            }
            break;
		}
        sleep(1);
    }

    for (int i = 0; i < NUM_THRS; i++) {
        // printf("[MAIN] Thread %d joined\n", i);
        pthread_join(gardeners[i], NULL);
    }

    cleanup();
}
