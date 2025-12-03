#ifndef FLOWER_H
#define FLOWER_H
#include <stdatomic.h>

// magic numbers
#define SHM_NAME "/flower_shm"
#define QUEUE_SIZE 20
#define NUM_PROCS 10
#define NUM_THRS 2
#define TIME_TO_WATER 1
#define TIME_WITHOUT_WATER 2

// sema names
#define SEM_QUEUE_MUTEX "/queue_mutex"
#define SEM_QUEUE_EMPTY "/queue_empty" 
#define SEM_QUEUE_FULL "/queue_full"
#define SEM_FLOWER_BASE "/flower_sem_"
#define SEM_ALIVE "/alive"

// msg queue
#define LOG_QUEUE "/flower_log_queue"
#define LOG_MSG_SIZE 1024L

enum state {
  WATERED,
  NEED_WATER,
  OVERWATERED,
  DEAD
};

struct Flower {
  unsigned int id;
  unsigned int time_to_water;
  unsigned int time_without_water;
  enum state st;
  int in_queue;
  int pid;
};

typedef struct {
    int front, rear;
    int data[QUEUE_SIZE];
} queue_t;

typedef struct {
    struct Flower flowers[NUM_PROCS];
    queue_t queue;
    int system_running;
    int alive_flowers;
} shared_mem_t;
#endif