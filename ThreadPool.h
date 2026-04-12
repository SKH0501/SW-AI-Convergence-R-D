#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <cstddef>

#define POOL_WAIT 3
#define POOL_NOWAIT 4
#define POOL_DISCARD 5
#define POOL_COMPLETE 6

typedef struct {
    void (*function)(void *param);
    void *param;
} task_t;

typedef enum {
    ON,
    OFF,
    STANDBY
} pool_state_t;

class ThreadPool {
private:
    pool_state_t state;

    task_t* q;
    int q_size;
    int q_front;
    int q_len;

    pthread_t* bee;
    int bee_size;

    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;

    static void* worker(void* param);

public:
    ThreadPool(size_t bee_size, size_t queue_size);
    ~ThreadPool();

    int submit(void (*f)(void *p), void *p, int flag);
    int shutdown(int how);
};

#endif