#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include "structs.h"

mqd_t mq;

volatile atomic_int running = 1;

void cleanup() {
    printf("[OBSERVER] Cleanup started\n");
   if (mq != (mqd_t)-1)
        mq_close(mq);
    mq_unlink(LOG_QUEUE);
    printf("[OBSERVER] exiting.\n");
}

void handle_sig(int sig) {
    running = 0;
}

int main() {
    printf("[OBSERVER] Starting observer...\n");

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    struct mq_attr attr;
    attr.mq_flags = O_NONBLOCK;
    attr.mq_maxmsg = 10L;
    attr.mq_msgsize = LOG_MSG_SIZE;
    attr.mq_curmsgs = 0L;

    mq_unlink(LOG_QUEUE);
    mq = mq_open(LOG_QUEUE, O_RDONLY | O_CREAT | O_NONBLOCK, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("[OBSERVER] mq_open failed");
        mq = mq_open(LOG_QUEUE, O_RDONLY | O_NONBLOCK);
        if (mq == (mqd_t)-1) {
            perror("[OBSERVER] mq_open (existing) also failed");
            exit(1);
        }
        printf("[OBSERVER] Opened existing queue\n");
    } else {
        printf("[OBSERVER] Created new queue\n");
    }

    char buf[LOG_MSG_SIZE];

    while (running) {
        // printf("DEBUG: Before mq_receive (running=%d)\n", running);
        ssize_t n = mq_receive(mq, buf, LOG_MSG_SIZE, NULL);
        // printf("DEBUG: After mq_receive, n=%ld, errno=%d\n", n, errno);
        if (n >= 0) {
            buf[n] = '\0';
            printf("[LOG] %s", buf);
        } else if (errno == EAGAIN) {
            sleep(1);
            mqd_t test = mq_open(LOG_QUEUE, O_RDONLY);
            if (test == (mqd_t)-1) {
                running = 0;
            }
        } else {
            perror("[OBSERVER] mq_receive");
            running = 0;
        }
        if (!running) {
            printf("[OBSERVER] Shutdown requested\n");
            break;
        }
    }
    cleanup();
    return 0;
}
