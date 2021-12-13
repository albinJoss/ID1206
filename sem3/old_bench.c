#define _GNU_SOURCE
#include "green.h"
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdatomic.h>

int flag = 0;
int loop = 0;
volatile atomic_int atomic_loop = 0;
atomic_int zero = 0;
green_cond_t cond;
green_mutex_t mutex;

pthread_cond_t pcond;
pthread_mutex_t pmutex;

unsigned long long cpumSecond()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double)tp.tv_sec * 1000000 + (double)tp.tv_usec);
}

void pthreads_init()
{
    atomic_init(&atomic_loop, 0);
    pthread_mutex_init(&pmutex, NULL);
    pthread_cond_init(&pcond, NULL);
}

void init()
{
    green_cond_init(&cond);
    green_mutex_init(&mutex);
}

void atomic_add()
{
    atomic_fetch_add_explicit(&atomic_loop, 1, memory_order_relaxed);
}

void atomic_sub()
{
    atomic_fetch_sub_explicit(&atomic_loop, 1, memory_order_relaxed);
}

void *test(void *arg)
{
    int id = *(int *)arg;
    
    while (loop > 0)
    {
        green_mutex_lock(&mutex);
        while (flag != id)
        {
            green_cond_wait(&cond, &mutex);
        }
        flag = (id + 1) % 2;
        green_cond_signal(&cond);
        green_mutex_unlock(&mutex);
        --loop;
    }
}

void *ptest(void *arg)
{
    int id = *(int *)arg;
    while (loop > 0)
    {
        
        pthread_mutex_lock(&pmutex);
        while (flag != id)
        {
            pthread_cond_wait(&pcond, &pmutex);
        }
        // printf("%d\n", id);
        flag = (id + 1) % 2;
        pthread_cond_signal(&pcond);
        pthread_mutex_unlock(&pmutex);
        //atomic_sub();
        --loop;
    }
    // pthread_exit(0);
}

int main()
{
    pthreads_init();
    init();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    CPU_SET(1, &cpuset);
    FILE *fptr;
    fptr = fopen("threads.txt", "w+");
    int a0 = 0;
    int a1 = 1;
    int pt0 = 0;
    int pt1 = 1;
    unsigned long long start, exectime, ptstart, ptexectime;
    // vår implementation
    for (int i = 1; i <= 10000; i++)
    {
        printf("%d\n", i);
        // atomic_fetch_add(&atomic_loop, i);
        loop = i;
        fprintf(fptr, "%d; ", loop);
        green_t g0, g1;

        start = cpumSecond();
        green_create(&g0, test, &a0);
        green_create(&g1, test, &a1);
        green_join(&g0, NULL);
        flag = 1;
        green_join(&g1, NULL);

        exectime = cpumSecond() - start;
        // printf("%d\n", i);
        //atomic_fetch_add(&atomic_loop, i);
        loop = i;
        // ptthread
        pthread_t ptt0 = pthread_self();
        pthread_t ptt1 = pthread_self();
        pthread_setaffinity_np(ptt0, sizeof(cpuset), &cpuset);
        pthread_setaffinity_np(ptt1, sizeof(cpuset), &cpuset);
        ptstart = cpumSecond();
        // pthread_create(&ptt0, NULL, ptest, &pt0);
        // pthread_create(&ptt1, NULL, ptest, &pt1);
        // pthread_join(ptt0, NULL);
        // flag = 1;
        // pthread_join(ptt1, NULL);
        ptexectime = cpumSecond() - ptstart;
        fprintf(fptr, "%llu; %llu\n", exectime, ptexectime);
        // printf("%d\n", i);
    }
    fflush(fptr);
    fclose(fptr);
    return 0;
}