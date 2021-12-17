#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include "green.h"

int num_threads;
uint64_t array_size;
unsigned int cores = 0;
float *x;
float a;

green_cond_t cond;
green_mutex_t mutex;
pthread_cond_t cv;
pthread_mutex_t lock;

typedef struct Args
{
    float *y;
    unsigned int thread;
} Args;

unsigned long long cpumSecond()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double)tp.tv_sec * 1000000 + (double)tp.tv_usec);
}

void setup()
{
    a = 4.0f;
    x = (float *)malloc(array_size * sizeof(float *));

    for (int i = 0; i < array_size; ++i)
    {
        x[i] = 2.0f;
    }
}

void *saxpy(void *arg)
{
    Args *args = arg;
    int per_thread = array_size / num_threads;
    int offset = args->thread * per_thread;
    for (int i = 0; i < per_thread; ++i)
    {
        args->y[i] += a * x[offset + i];
    }
}

void green_test()
{
    green_t threads[num_threads];
    int per_thread = array_size / num_threads;
    Args *arg[num_threads];

    for (int i = 0; i < num_threads; ++i)
    {
        arg[i] = (Args *)malloc(sizeof(Args *));
        arg[i]->y = (float *)malloc(per_thread * sizeof(float *));
        arg[i]->thread = i;
    }

    for (int i = 0; i < per_thread; ++i)
    {
        for (int j = 0; j < num_threads; ++j)
        {
            arg[j]->y[i] = 2.0f;
        }
    }

    unsigned long long start = cpumSecond();
    for (int i = 0; i < num_threads; ++i)
    {
        green_create(&threads[i], saxpy, arg[i]);
    }
    for (int i = 0; i < num_threads; ++i)
    {
        green_join(&threads[i], NULL);
        free(arg[i]->y);
        free(arg[i]);
    }
    unsigned long long end = cpumSecond() - start;
    printf("%llu\n", end);
}

void pthreads_test()
{
    pthread_t threads[num_threads];
    cpu_set_t cpuset;
    int per_thread = array_size / num_threads;
    Args *args[num_threads];

    for (int i = 0; i < cores; ++i)
    {
        CPU_SET(i, &cpuset);
    }

    for (int i = 0; i < num_threads; ++i)
    {
        threads[i] = pthread_self();
        // pthread_setaffinity_np(threads[i], sizeof(cpuset), &cpuset);

        args[i] = (Args *)malloc(sizeof(Args *));
        args[i]->y = (float *)malloc(per_thread * sizeof(float *));
        args[i]->thread = i;
    }

    for (int i = 0; i < per_thread; ++i)
    {
        for (int j = 0; j < num_threads; ++j)
        {
            args[j]->y[i] = 2.0f;
        }
    }

    unsigned long long start = cpumSecond();

    for (int i = 0; i < num_threads; ++i)
    {
        pthread_create(&threads[i], NULL, saxpy, (void *)args[i]);
    }
    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
        free(args[i]->y);
        free(args[i]);
    }
    unsigned long long end = cpumSecond() - start;
    printf("%llu\n", end);
}

int main(int argc, char *argv[])
{
    num_threads = argc > 1 ? *argv[1] : 2;
    array_size = argc > 2 ? *argv[2] : 100000;
    if (strcasecmp(argv[3], "max") && argc > 3)
    {
        cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (cores < 1)
        {
            printf("Could not find how many cores there are. Setting it to 4.\n");
            cores = 4;
        }
    }
    else
    {
        cores = argc > 3 ? *argv[3] : num_threads;
    }
    if (argc < 3)
    {
        int max_cores = sysconf(_SC_NPROCESSORS_ONLN);
        printf("Tested\n");
        while (cores <= max_cores)
        {
            printf("%u\n", cores);
            num_threads = cores;
            do
            {
                setup();
                // printf("%lu;", array_size);
                pthreads_test();

                //green_test();

                free(x);

                array_size += 100000;

            } while (array_size <= 10000000);
            array_size = 100000;
            cores += 2;
        }
    }
    else
    {
        do
        {
            setup();
            // printf("%lu;", array_size);
            pthreads_test();

            green_test();

            free(x);

            array_size += 100000;

        } while (array_size <= 10000000);
    }

    return 0;
}