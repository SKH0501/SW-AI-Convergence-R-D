#include "ThreadPool.h"

#include <algorithm>
#include <stdexcept>

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

        (t.function)(t.param);
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
      is_shutdown(false) {
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

        for (int i = 0; i < created; ++i) {
            pthread_join(bee[i], nullptr);
        }

        pthread_cond_destroy(&empty);
        pthread_cond_destroy(&full);
        pthread_mutex_destroy(&mutex);
        throw;
    }
}

ThreadPool::~ThreadPool() {
    shutdown(POOL_COMPLETE);
}

int ThreadPool::submit(void (*f)(void* p), void* p, int flag) {
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
    q_len++;

    pthread_cond_signal(&full);
    pthread_mutex_unlock(&mutex);

    return 0;
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
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < bee_size; ++i) {
        pthread_join(bee[i], nullptr);
    }

    pthread_cond_destroy(&empty);
    pthread_cond_destroy(&full);
    pthread_mutex_destroy(&mutex);

    return 0;
}