#include <assert.h>
#include <bits/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "thread_pool.h"


struct thread_work_t* create_thread_work(client_handler_t ch, int32_t cfd) {
    struct thread_work_t* tw = calloc(1, sizeof(struct thread_work_t));
    if (tw == NULL) {
        printf("create_thread_work: calloc error\n");
        return NULL;
    }

    memset(tw, 0, sizeof(struct thread_work_t));

    tw->client_handler = ch;
    tw->client_fd = cfd;
    tw->prev = NULL;
    
    return tw;
}

void destory_thread_work(struct thread_work_t* tw) {
    if (tw == NULL) return;
    free(tw);
}

int32_t enqueue_thread_work(struct thread_pool_t* tp, client_handler_t ch, 
        int32_t cfd) {
    if (tp == NULL) {
        printf("enqueue_thread_work: NULL thread pool\n");
        return 0;
    }

    struct thread_work_t* tw = create_thread_work(ch, cfd);
    if (tw == NULL) {
        return 0;
    }

    int32_t rval = -1;

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec = 60;

    /** Critical Section */
    if ((rval = pthread_mutex_timedlock(&tp->queue_mutex, &deadline)) != 0) {
        printf("enqueue_thread_work: mutex lock timeout (%d). abort work \
                enqueue\n", rval);
        return rval;
    }

    if (tp->queue_size == 0) {
        assert(tp->first == NULL && tp->last == NULL);
        tp->first = tw;
        tp->last  = tw;
    } else {
        assert(tp->last != NULL && tp->last->prev == NULL);
        tp->last->prev = tw;
        tp->last = tw;
    }
    tp->queue_size++;

    int32_t r = 1;
    if (pthread_cond_broadcast(&tp->enqueue_cond) != 0) {
        printf("enqueue_thread_work: broadcast error (%d).\n", rval);
        r = rval;
    }
    if ((rval = pthread_mutex_unlock(&tp->queue_mutex)) != 0) {
        printf("enqueue_thread_work: mutex unlock error (%d).\n", rval);
        r = rval;
    }
    /** Critical Section */

    return r;
}

struct thread_work_t* dequeue_thread_work(struct thread_pool_t* tp, char* log, 
        int32_t lfd) {
    assert(log != NULL);

    pthread_t tid = pthread_self();

    if (tp == NULL) {
        sprintf(log, "(thread %lu) dequeue_thread_work: NULL thread_pool\n",
                tid);
        write(lfd, log, LOG_MSG_SIZE * sizeof(char));
        return NULL;
    }

    struct thread_work_t* tw;

    if (tp->queue_size == 0) {
        assert(tp->first == NULL && tp->last == NULL);
        tw = NULL;
    } else if (tp->queue_size == 1) {
        assert(tp->first != NULL && tp->first == tp->last && 
                tp->first->prev == NULL);
        tw = tp->first;
        tp->first = tp->last = NULL;
        tp->queue_size--;
    } else {
        assert(tp->first != NULL && tp->first->prev != NULL);
        tw = tp->first;
        tp->first = tp->first->prev;
        tp->queue_size--;
    }

    return tw;
}

void* worker_main(void* args) {
    struct thread_pool_t* tp = args;
    struct thread_work_t* tw = NULL;

    pthread_t tid = pthread_self();

    char logf[LOG_NAME_SIZE];
    char log[LOG_MSG_SIZE];
    memset(logf, 0, LOG_NAME_SIZE * sizeof(char));
    memset(log, 0, LOG_MSG_SIZE * sizeof(char));

    sprintf(logf, "%ld.tlog", tid);
    int32_t lfd = open(logf, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0644);
    if (lfd == -1) {
        return NULL;
    }

    if (tp == NULL) {
        sprintf(log, "(thread %lu) worker_main: receive NULL thread pool", tid);
        write(lfd, log, LOG_MSG_SIZE);
        close(lfd);
        return NULL;
    }

    int32_t rval = -1;
    struct timespec deadline;

    while (1) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec = 60;
        
        /** Critical Section */
        if ((rval = pthread_mutex_timedlock(&tp->queue_mutex, &deadline)) != 0) {
            sprintf(log, "(thread %ld) worker_main: mutex lock timeout (%d)",
                    tid, rval);
            write(lfd, log, LOG_MSG_SIZE);
            continue;
        }

        while (tp->first == NULL) {
            assert(tp->queue_size == 0);
            if ((rval = pthread_cond_wait(&tp->enqueue_cond, &tp->queue_mutex)) 
                    != 0) {
                sprintf(log, "(thread %ld) worker_main: pthread_cond_wait \
                        error (%d).\n", tid, rval);
                write(lfd, log, LOG_MSG_SIZE);
                close(lfd);
                exit(1);
            }
        }

        if (tp->stop) {
            break;
        }

        tw = dequeue_thread_work(tp, log, lfd);
        if ((rval = pthread_mutex_unlock(&tp->queue_mutex)) != 0) {
            sprintf(log, "(thread %ld) worker_main: mutex unlock error (%d).\n",
                    tid, rval);
            write(lfd, log, LOG_MSG_SIZE);
            close(lfd);
            exit(1);
        }
        /** Critical Section */

        if (tw != NULL) {
            tw->client_handler(tw->client_fd, lfd);
            destory_thread_work(tw);
        }
    }

    tp->active_thread--;
    assert(tp->active_thread >= 0);
    pthread_mutex_unlock(&tp->queue_mutex);

    return NULL;
}

struct thread_pool_t* create_thread_pool() {
    struct thread_pool_t* tp = calloc(1, sizeof(struct thread_pool_t));
    if (tp == NULL) {
        printf("create_thread_pool: calloc error\n");
        return NULL;
    }

    int32_t rval = -1;
    if ((rval = pthread_mutex_init(&tp->queue_mutex, NULL)) != 0) {
        printf("create_thread_pool: mutex init error\n");
        free(tp);
        return NULL;
    }
    
    if((rval = pthread_cond_init(&tp->enqueue_cond, NULL)) != 0) {
        printf("create_thread_pool: mutex init error\n");
        destory_thread_pool(tp);
        return NULL;
    }

    tp->queue_size = 0;
    tp->first = NULL;
    tp->last = NULL;

    pthread_t tid = 0;
    for (uint8_t i = 0; i < THREAD_LIMIT; i++) {
        if ((rval = pthread_create(&tid, NULL, worker_main, tp)) != 0) {
            printf("create_thread_pool: thread creation error (%d).\n", rval);
            continue;
        }
        tp->active_thread++;
        if ((rval = pthread_detach(tid)) != 0) {
            printf("create_threa_pool: thread detachment erro (%d).\n", rval);
        }
    }

    return tp;
}

int32_t destory_thread_pool(struct thread_pool_t* tp) {
    if (tp == NULL) {
        printf("destory_thread_pool: NULL thread pool\n");
        return 1;
    }

    int32_t rval;
    struct thread_work_t* curr = tp->first;
    struct thread_work_t* prev = NULL;

    if ((rval = pthread_mutex_lock(&tp->queue_mutex)) != 0) {
        printf("destory_thread_pool: mutex lock error (%d)\n", rval);
        return 0;
    }

    while (curr != NULL) {
        prev = curr->prev;
        destory_thread_work(curr);
        curr = prev;
    }

    tp->first = tp->last = NULL;
    tp->stop = 1;

    int32_t r = 1;
    if ((rval = pthread_cond_broadcast(&tp->enqueue_cond)) != 0) {
        printf("destory_thread_pool: broadcast error (%d)\n", rval);
        return 0;
    }

    if ((rval = pthread_mutex_unlock(&tp->queue_mutex)) != 0) {
        printf("destory_thread_pool: mutex unlock error (%d)\n", rval);
        return 0;
    }

    /** Wait */

    if ((rval = pthread_mutex_destroy(&tp->queue_mutex)) != 0) {
        printf("destory_thread_pool: mutex destory error (%d)\n", rval);
        r = 0;
    }
    if ((rval = pthread_cond_destroy(&tp->enqueue_cond)) != 0) {
        printf("destory_thread_pool: mutex destory error (%d)\n", rval);
        r = 0;
    }

    free(tp);

    return r;
}

void thread_pool_wait(struct thread_pool_t* tp) {
    // TODO
}
