/*
 * Copyright(c) 2021-2024 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "pthread_pool.h"

/*
 * 스레드풀에 상주하는 일꾼(일벌) 스레드가 수행할 함수입니다.
 * 대기열에 작업이 있으면 꺼내서 실행하고, 없으면 새 작업이 들어올 때까지 기다립니다.
 * 스레드풀이 종료 단계에 들어가면 (OFF 또는 STANDBY 상태), 남은 작업 처리 여부에 따라
 * 즉시 종료하거나(OFF), 대기열이 비워질 때까지 처리한 뒤 종료(STANDBY)합니다.
 */
static void *worker(void *param)
{
    pthread_pool_t *pool = (pthread_pool_t *)param;
    task_t t;

    while (1) {
        /* 뮤텍스 잠금 */
        pthread_mutex_lock(&pool->mutex);

        /* 대기열이 비어 있고, 여전히 실행 중이면 계속 기다린다 */
        while (pool->q_len == 0 && pool->state == ON) {
            pthread_cond_wait(&pool->full, &pool->mutex);
        }

        /* 
         * 1) DISCARD 모드(OFF)로 들어왔을 때: 즉시 빠져나와 종료 
         * 2) COMPLETE 모드(STANDBY)로 들어왔고, 대기열이 비어 있으면 종료 
         */
        if (pool->state == OFF || (pool->state == STANDBY && pool->q_len == 0)) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* 대기열에서 작업 꺼내기 */
        t = pool->q[pool->q_front];
        pool->q_front = (pool->q_front + 1) % pool->q_size;
        pool->q_len--;

        /* 작업을 꺼냈으니, submit 쪽에서 대기 중인 스레드를 깨워야 할 수도 있다 */
        pthread_cond_signal(&pool->empty);
        pthread_mutex_unlock(&pool->mutex);

        /* 꺼낸 작업 실행 */
        (t.function)(t.param);
        /* 반복해서 다음 작업을 처리 */
    }

    /* 스레드 종료 */
    pthread_exit(NULL);
    return NULL;
}

/*
 * 스레드풀을 생성 및 초기화합니다.
 * bee_size: 일꾼 스레드 수 (최대 POOL_MAXBSIZE)
 * queue_size: 대기열 크기 (최대 POOL_MAXQSIZE)
 * queue_size < bee_size인 경우 queue_size를 bee_size로 상향 조정합니다.
 * 성공하면 POOL_SUCCESS, 실패하면 POOL_FAIL을 리턴합니다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
    size_t i;

    /* 1) 파라미터 검사 */
    if (bee_size > POOL_MAXBSIZE || queue_size > POOL_MAXQSIZE) {
        return POOL_FAIL;
    }

    /* 2) queue_size가 bee_size보다 작으면 상향 조정 */
    if (queue_size < bee_size) {
        queue_size = bee_size;
    }

    /* 3) 메모리 할당: 작업 큐(q)와 일꾼 스레드 배열(bee) */
    pool->q = (task_t*)malloc(sizeof(task_t) * queue_size);
    if (pool->q == NULL) {
        return POOL_FAIL;
    }
    pool->bee = (pthread_t*)malloc(sizeof(pthread_t) * bee_size);
    if (pool->bee == NULL) {
        free(pool->q);
        return POOL_FAIL;
    }

    /* 4) 제어블록 필드 초기화 */
    pool->state = ON;                /* 실행 중 상태 */
    pool->bee_size = bee_size;
    pool->q_size = queue_size;
    pool->q_front = 0;
    pool->q_len = 0;

    /* 뮤텍스 및 조건변수 초기화 */
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool->q);
        free(pool->bee);
        return POOL_FAIL;
    }
    if (pthread_cond_init(&pool->full, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool->q);
        free(pool->bee);
        return POOL_FAIL;
    }
    if (pthread_cond_init(&pool->empty, NULL) != 0) {
        pthread_cond_destroy(&pool->full);
        pthread_mutex_destroy(&pool->mutex);
        free(pool->q);
        free(pool->bee);
        return POOL_FAIL;
    }

    /* 5) 일꾼 스레드 생성 */
    for (i = 0; i < bee_size; i++) {
        if (pthread_create(&pool->bee[i], NULL, worker, pool) != 0) {
            /* 실패 시, 지금까지 생성된 스레드는 종료시켜야 한다 */
            size_t j;
            pool->state = OFF;                /* 즉시 종료 모드로 전환 */
            pthread_cond_broadcast(&pool->full);
            pthread_cond_broadcast(&pool->empty);
            for (j = 0; j < i; j++) {
                pthread_join(pool->bee[j], NULL);
            }
            pthread_cond_destroy(&pool->empty);
            pthread_cond_destroy(&pool->full);
            pthread_mutex_destroy(&pool->mutex);
            free(pool->q);
            free(pool->bee);
            return POOL_FAIL;
        }
    }

    return POOL_SUCCESS;
}

/*
 * 스레드풀에 작업을 제출합니다.
 * f: 실행할 함수 포인터, p: f로 넘어갈 인자
 * flag: 대기열이 가득 찼을 때 동작 (POOL_WAIT이면 대기, POOL_NOWAIT이면 즉시 반환)
 * 스레드풀이 ON 상태가 아니면 POOL_FAIL을 리턴합니다.
 * 새로 enqueue에 성공하면 POOL_SUCCESS, 가득 차고 flag=POOL_NOWAIT이면 POOL_FULL을 리턴합니다.
 */
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag)
{
    /* 1) 뮤텍스 잠금 */
    pthread_mutex_lock(&pool->mutex);

    /* 2) pool이 이미 종료 중(OFF 또는 STANDBY)이면 작업을 받지 않는다 */
    if (pool->state != ON) {
        pthread_mutex_unlock(&pool->mutex);
        return POOL_FAIL;
    }

    /* 3) 대기열이 가득 찬 상황 처리 */
    if (pool->q_len == pool->q_size) {
        if (flag == POOL_NOWAIT) {
            pthread_mutex_unlock(&pool->mutex);
            return POOL_FULL;
        }
        /* flag == POOL_WAIT인 경우, 빈 공간이 생길 때까지 기다린다 */
        while (pool->q_len == pool->q_size && pool->state == ON) {
            pthread_cond_wait(&pool->empty, &pool->mutex);
        }
        /* 대기하면서 상태가 변경되어 더 이상 ON이 아니면 실패 */
        if (pool->state != ON) {
            pthread_mutex_unlock(&pool->mutex);
            return POOL_FAIL;
        }
        /* 빈 공간이 생겼으므로 아래로 내려와 작업을 삽입 */
    }

    /* 4) 대기열에 작업 삽입 */
    {
        int insert_pos = (pool->q_front + pool->q_len) % pool->q_size;
        pool->q[insert_pos].function = f;
        pool->q[insert_pos].param = p;
        pool->q_len++;
    }

    /* 5) 작업이 채워졌으니, 기다리던 worker 스레드를 깨운다 */
    pthread_cond_signal(&pool->full);
    pthread_mutex_unlock(&pool->mutex);
    return POOL_SUCCESS;
}

/*
 * 스레드풀을 종료합니다.
 * how == POOL_COMPLETE  → 대기열에 남은 작업을 모두 수행한 뒤 종료
 * how == POOL_DISCARD   → 대기열에 남은 작업을 버리고 즉시 종료
 * 종료 요청이 들어오면 더 이상 새로운 작업을 받지 않고, 모든 worker를 깨운 뒤 join,
 * 마지막으로 할당된 자원을 해제합니다.
 */
int pthread_pool_shutdown(pthread_pool_t *pool, int how)
{
    size_t i;

    /* 1) 뮤텍스 잠금 */
    pthread_mutex_lock(&pool->mutex);

    /* 2) 상태 전환: OFF=즉시 종료(버림), STANDBY=대기열 비운 뒤 종료 */
    if (how == POOL_DISCARD) {
        pool->state = OFF;
    } else {
        pool->state = STANDBY;
    }

    /* 3) 작업이 없어도 대기 중인 모든 worker와 submit 대기 중인 스레드를 깨운다 */
    pthread_cond_broadcast(&pool->full);
    pthread_cond_broadcast(&pool->empty);
    pthread_mutex_unlock(&pool->mutex);

    /* 4) 모든 worker 스레드를 join */
    for (i = 0; i < (size_t)pool->bee_size; i++) {
        pthread_join(pool->bee[i], NULL);
    }

    /* 5) 할당된 자원 해제 (뮤텍스/조건변수는 destroy하지 않는다) */
    free(pool->q);
    free(pool->bee);

    return POOL_SUCCESS;
}

