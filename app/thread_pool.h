#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdint.h>
#include <pthread.h>

#include "type.h"

#define THREAD_LIMIT 4

#define LOG_NAME_SIZE 64
#define LOG_MSG_SIZE  128


typedef void(* client_handler_t)(int32_t, int32_t);


struct thread_pool_t {
    uint32_t            queue_size;
    uint8_t             active_thread;
    int8_t              stop;
    pthread_mutex_t     queue_mutex;
    pthread_cond_t      enqueue_cond;
    struct thread_work_t* first;
    struct thread_work_t* last;
};

struct thread_work_t {
    client_handler_t      client_handler;
    int32_t               client_fd;
    struct thread_work_t* prev;
};


struct thread_work_t* create_thread_work(client_handler_t, int32_t);

void destory_thread_work(struct thread_work_t*);

/** Called by main thread */
int32_t enqueue_thread_work(struct thread_pool_t*, client_handler_t, int32_t);

/** Called by worker thread */
struct thread_work_t* dequeue_thread_work(struct thread_pool_t*, char*, int32_t);

void* worker_main(void*);

/** Call by main thread */
struct thread_pool_t* create_thread_pool();

/** Call by main thread */
int32_t destory_thread_pool(struct thread_pool_t*);

void thread_pool_wait(struct thread_pool_t*);

#endif // !THREAD_POOL_H
