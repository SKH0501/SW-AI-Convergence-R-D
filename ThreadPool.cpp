#include "ThreadPool.h"
#include <cstdlib>

void* ThreadPool::worker(void* param) {
    ThreadPool* pool = (ThreadPool*)param;
    task_t t;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->q_len == 0 && pool->state == ON) {
            pthread_cond_wait(&pool->full, &pool->mutex);
        }

        if (pool->state == OFF || (pool->state == STANDBY && pool->q_len == 0)) {
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

    pthread_exit(NULL);
    return NULL;
}

ThreadPool::ThreadPool(size_t bee_size, size_t queue_size) {
    if (queue_size < bee_size) {
        queue_size = bee_size;
    }

    state = ON;

    this->bee_size = bee_size;
    q_size = queue_size;

    q_front = 0;
    q_len = 0;

    q = (task_t*)malloc(sizeof(task_t) * q_size);
    bee = (pthread_t*)malloc(sizeof(pthread_t) * bee_size);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);

    for (size_t i = 0; i < bee_size; i++) {
        pthread_create(&bee[i], NULL, worker, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown(POOL_COMPLETE);
}

int ThreadPool::submit(void (*f)(void *p), void *p, int flag) {
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

    int pos = (q_front + q_len) % q_size;
    q[pos].function = f;
    q[pos].param = p;
    q_len++;

    pthread_cond_signal(&full);
    pthread_mutex_unlock(&mutex);

    return 0;
}

int ThreadPool::shutdown(int how) {
    pthread_mutex_lock(&mutex);

    if (how == POOL_DISCARD) {
        state = OFF;
    } else {
        state = STANDBY;
    }

    pthread_cond_broadcast(&full);
    pthread_cond_broadcast(&empty);

    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < bee_size; i++) {
        pthread_join(bee[i], NULL);
    }

    free(q);
    free(bee);

    return 0;
}