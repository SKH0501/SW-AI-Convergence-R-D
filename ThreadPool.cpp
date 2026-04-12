#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t bee_size, size_t queue_size) {
    pthread_pool_init(&pool, bee_size, queue_size);
}

ThreadPool::~ThreadPool() {
}

int ThreadPool::submit(void (*f)(void *p), void *p, int flag) {
    return pthread_pool_submit(&pool, f, p, flag);
}

int ThreadPool::shutdown(int how) {
    return pthread_pool_shutdown(&pool, how);
}