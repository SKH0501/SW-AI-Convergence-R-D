#include "ThreadPool.h"
#include <cstdio>
#include <unistd.h>

void task(void* arg) {
    int num = *static_cast<int*>(arg);
    std::printf("Task %d executed\n", num);
    delete static_cast<int*>(arg);
}

int main() {
    ThreadPool pool(4, 10);

    for (int i = 0; i < 5; ++i) {
        int* arg = new int(i);
        pool.submit(task, arg, POOL_WAIT);
    }

    sleep(1);
    return 0;
}