#include "ThreadPool.h"

#include <algorithm>
#include <stdexcept>
#include <chrono>

static long long now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void* ThreadPool::worker(void* param) {
    auto* pool = static_cast<ThreadPool*>(param);
    Task t{};

    while (true) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->q_len == 0 && pool->state == ON) {
            pthread_cond_wait(&pool->full, &pool->mutex);
        }

        if (pool->state == OFF ||
            (pool->state == STANDBY && pool->q_len == 0)) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        t = pool->q[pool->q_front];
        pool->q_front = (pool->q_front + 1) % pool->q_size;
        pool->q_len--;

        pthread_cond_signal(&pool->empty);
        pthread_mutex_unlock(&pool->mutex);

        if (t.metric != nullptr) {
            t.metric->start_time_ns = now_ns();
        }

        (t.function)(t.param);

        if (t.metric != nullptr) {
            t.metric->end_time_ns = now_ns();
        }

        pthread_mutex_lock(&pool->mutex);

        pool->completed_count++;

        if (pool->completed_count == pool->submitted_count) {
            pthread_cond_signal(&pool->all_done);
        }

        pthread_mutex_unlock(&pool->mutex);
    }

    return nullptr;
}

ThreadPool::ThreadPool(std::size_t bee_size_arg, std::size_t queue_size_arg)
    : state(ON),
      q_size(static_cast<int>(std::max(bee_size_arg, queue_size_arg))),
      q_front(0),
      q_len(0),
      q(q_size),
      bee_size(static_cast<int>(bee_size_arg)),
      bee(bee_size),
      is_shutdown(false),
      submitted_count(0),
      completed_count(0) {
    if (pthread_mutex_init(&mutex, nullptr) != 0) {
        throw std::runtime_error("pthread_mutex_init failed");
    }

    if (pthread_cond_init(&full, nullptr) != 0) {
        pthread_mutex_destroy(&mutex);
        throw std::runtime_error("pthread_cond_init(full) failed");
    }

    if (pthread_cond_init(&empty, nullptr) != 0) {
        pthread_cond_destroy(&full);
        pthread_mutex_destroy(&mutex);
        throw std::runtime_error("pthread_cond_init(empty) failed");
    }

    if (pthread_cond_init(&all_done, nullptr) != 0) {
        pthread_cond_destroy(&empty);
        pthread_cond_destroy(&full);
        pthread_mutex_destroy(&mutex);
        throw std::runtime_error("pthread_cond_init(all_done) failed");
    }

    int created = 0;

    try {
        for (int i = 0; i < bee_size; ++i) {
            if (pthread_create(&bee[i], nullptr, worker, this) != 0) {
                throw std::runtime_error("pthread_create failed");
            }

            ++created;
        }
    } catch (...) {
        state = OFF;

        pthread_cond_broadcast(&full);
        pthread_cond_broadcast(&empty);
        pthread_cond_broadcast(&all_done);

        for (int i = 0; i < created; ++i) {
            pthread_join(bee[i], nullptr);
        }

        pthread_cond_destroy(&all_done);
        pthread_cond_destroy(&empty);
        pthread_cond_destroy(&full);
        pthread_mutex_destroy(&mutex);

        throw;
    }
}

ThreadPool::~ThreadPool() {
    shutdown(POOL_COMPLETE);
}

int ThreadPool::submit(void (*f)(void* p), void* p, int flag, TaskMetric* metric) {
    if (metric != nullptr) {
        metric->submit_time_ns = now_ns();
    }

    pthread_mutex_lock(&mutex);

    if (state != ON) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    if (q_len == q_size) {
        if (flag == POOL_NOWAIT) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        while (q_len == q_size && state == ON) {
            pthread_cond_wait(&empty, &mutex);
        }

        if (state != ON) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }

    const int pos = (q_front + q_len) % q_size;

    q[pos].function = f;
    q[pos].param = p;
    q[pos].metric = metric;

    q_len++;
    submitted_count++;

    pthread_cond_signal(&full);
    pthread_mutex_unlock(&mutex);

    return 0;
}

void ThreadPool::wait_all() {
    pthread_mutex_lock(&mutex);

    while (completed_count < submitted_count) {
        pthread_cond_wait(&all_done, &mutex);
    }

    pthread_mutex_unlock(&mutex);
}

void ThreadPool::reset_counter() {
    pthread_mutex_lock(&mutex);

    submitted_count = 0;
    completed_count = 0;

    pthread_mutex_unlock(&mutex);
}

int ThreadPool::get_queue_length() {
    pthread_mutex_lock(&mutex);

    int current_q_len = q_len;

    pthread_mutex_unlock(&mutex);

    return current_q_len;
}

int ThreadPool::shutdown(int how) {
    pthread_mutex_lock(&mutex);

    if (is_shutdown) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    is_shutdown = true;
    state = (how == POOL_DISCARD) ? OFF : STANDBY;

    pthread_cond_broadcast(&full);
    pthread_cond_broadcast(&empty);
    pthread_cond_broadcast(&all_done);

    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < bee_size; ++i) {
        pthread_join(bee[i], nullptr);
    }

    pthread_cond_destroy(&all_done);
    pthread_cond_destroy(&empty);
    pthread_cond_destroy(&full);
    pthread_mutex_destroy(&mutex);

    return 0;
}