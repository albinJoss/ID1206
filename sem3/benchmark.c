#include "green.h"
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

int flag = 0;
int loop = 50000;
green_cond_t cond;
green_mutex_t mutex;

pthread_cond_t pcond;
pthread_mutex_t pmutex;

unsigned long long cpumSecond() {
   struct timeval tp;
   gettimeofday(&tp,NULL);
   return ((double)tp.tv_sec * 1000000 + (double)tp.tv_usec);
}
void *test(void *arg)
{
    int id = *(int *)arg;
    while (loop > 0)
    {
        //printf("1st lock");
        green_mutex_lock(&mutex);
        //printf("thread %d: %d\n", id, loop);
        while (flag != id)
        {
            //green_mutex_unlock(&mutex); must be removed bc mutex becomes unprotected
            green_cond_wait(&cond, &mutex);
             //printf("2nd lock");
           // green_mutex_lock(&mutex);
        }
        flag = (id + 1) % 2;
        green_cond_signal(&cond);
        green_mutex_unlock(&mutex);
        loop--;
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
        flag = (id + 1) % 2;
        pthread_cond_signal(&pcond);
        pthread_mutex_unlock(&pmutex);
        loop--;
    }
}

void pthreads_init()
{
    pthread_mutex_init(&pmutex, NULL);
    pthread_cond_init(&pcond, NULL);
}

int main()
{
//vår implementation
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;
    unsigned long long start = cpumSecond();
    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);
    green_join(&g0, NULL);
    green_join(&g1, NULL);
    unsigned long long exectime = cpumSecond() - start;
    printf("time: \n");
    printf("%llu\n", exectime);

    loop = 50000;
    flag = 0;
//ptthread
    pthreads_init();
    pthread_t ptt0, ptt1;
    int pt0 = 0;
    int pt1 = 1;
    start = cpumSecond();
    pthread_create(&ptt0, NULL, ptest, &pt0);
    pthread_create(&ptt1, NULL, ptest, &pt1);
    pthread_join(ptt0, NULL);
    pthread_join(ptt1, NULL);
    exectime = cpumSecond() - start;
    printf("ptime: \n");
    printf("%llu\n", exectime);
    return 0;
}