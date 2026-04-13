#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <cstddef>
#include <vector>

#define POOL_WAIT 3
#define POOL_NOWAIT 4
#define POOL_DISCARD 5
#define POOL_COMPLETE 6

struct Task {
    void (*function)(void* param) = nullptr;
    void* param = nullptr;
};

enum PoolState {
    ON,
    OFF,
    STANDBY
};

class ThreadPool {
private:
    PoolState state;

    int q_size;
    int q_front;
    int q_len;
    std::vector<Task> q;

    int bee_size;
    std::vector<pthread_t> bee;

    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;

    bool is_shutdown;

    static void* worker(void* param);

public:
    explicit ThreadPool(std::size_t bee_size, std::size_t queue_size);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    int submit(void (*f)(void* p), void* p, int flag);
    int shutdown(int how);
};

#endif