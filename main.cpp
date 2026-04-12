#include "ThreadPool.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

void task(void* arg) {
    int num = *(int*)arg;
    printf("Task %d executed\n", num);
    free(arg);
}

int main() {
    ThreadPool pool(4, 10);

    for (int i = 0; i < 5; i++) {
        int* arg = (int*)malloc(sizeof(int));
        *arg = i;
        pool.submit(task, arg, POOL_WAIT);
    }

    sleep(1);

    return 0;
}