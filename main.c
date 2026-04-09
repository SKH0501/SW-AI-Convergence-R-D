#include "pthread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void task(void* arg) {
    int num = *(int*)arg;
    printf("Task %d executed\n", num);
}

int main() {
    pthread_pool_t pool;

    pthread_pool_init(&pool, 4, 10);

    for (int i = 0; i < 5; i++) {
        int* arg = (int*)malloc(sizeof(int));
        *arg = i;
        pthread_pool_submit(&pool, task, arg, 0);
    }

    sleep(1);

    pthread_pool_shutdown(&pool, 0);

    return 0;
}