#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "pthread_pool.h"

class ThreadPool {
private:
    pthread_pool_t pool;

public:
    ThreadPool(size_t bee_size, size_t queue_size);
    ~ThreadPool();

    int submit(void (*f)(void *p), void *p, int flag);
    int shutdown(int how);
};

#endif