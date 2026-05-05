#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <cstddef>
#include <vector>
#include <atomic>

#define POOL_WAIT 3
#define POOL_NOWAIT 4
#define POOL_DISCARD 5
#define POOL_COMPLETE 6

struct TaskMetric {
    std::atomic<long long> submit_time_ns;
    std::atomic<long long> start_time_ns;
    std::atomic<long long> end_time_ns;

    TaskMetric()
        : submit_time_ns(0),
          start_time_ns(0),
          end_time_ns(0) {}
};

struct Task {
    void (*function)(void* param) = nullptr;
    void* param = nullptr;
    TaskMetric* metric = nullptr;
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

    // wait_all용 조건변수
    pthread_cond_t all_done;

    bool is_shutdown;

    int submitted_count;
    int completed_count;

    static void* worker(void* param);

public:
    explicit ThreadPool(std::size_t bee_size, std::size_t queue_size);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    int submit(void (*f)(void* p), void* p, int flag, TaskMetric* metric = nullptr);

    void wait_all();
    void reset_counter();

    int shutdown(int how);
};

#endif