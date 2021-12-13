#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "green.h"

green_mutex_t lock;
green_cond_t cv;
int zero = 0;
int one = 0;
int shared = 0;
int flag = 0;
int loop = 1;
int number0 = 0;
int number1 = 1;

void init()
{
    green_mutex_init(&lock);
    green_cond_init(&cv);
}

void *test_cj(void *arg)
{
    int gid = *(int *)arg;
    printf("Thread: %d\n", gid);
    if(gid == 0)
    {
        ++number0;
    }
    else
    {
        ++number1;
    }
}

void *test_mutex(void *arg)
{
    int gid = *(int *)arg;

    green_mutex_lock(&lock);
    
    if(gid == 0)
    {
        ++zero;
    }
    else if(gid == 1)
    {
        ++one;
    }
    ++shared;

    green_mutex_unlock(&lock);
}

void *test_cond(void *arg)
{
    int gid = *(int *)arg;
    
    green_mutex_lock(&lock);
    while(flag != gid)
    {
        green_cond_wait(&cv, &lock);

    }
    flag = (flag + 1) % 2;
    if(gid == 0)
    {
        --loop;
    }
    else
    {
        ++loop;
    }
    green_cond_signal(&cv);
    green_mutex_unlock(&lock);
}

void create_join(int tests)
{
    printf("%d\n", tests);
    green_t gt0, gt1;
    int arg0 = 0;
    int arg1 = 1;
    for(int i = 0; i < tests; ++i)
    {
        green_create(&gt0, test_cj, &arg0);
        green_create(&gt1, test_cj, &arg1);
        green_join(&gt0, NULL);
        green_join(&gt1, NULL); 
        // printf("arg0: %d\targ1: %d\n", arg0, arg1);
        assert(number1 == number0 + 1);
    }
}

void mutex(int tests)
{
    green_t gt0, gt1;
    int arg0 = 0;
    int arg1 = 1;
    int savedone, savedzero;
    for(int i = 0; i < tests; ++i)
    {
        savedone = one;
        savedzero = zero;

        green_create(&gt0, test_mutex, &arg0);
        green_create(&gt1, test_mutex, &arg1);
        green_join(&gt0, (void *) &arg0);
        green_join(&gt1, (void *) &arg1); 
        
        assert(one == savedone + 1);
        assert(zero == savedzero + 1);
        assert(shared == zero + one);
    }
}

void cond(int tests)
{
    green_t gt0, gt1;
    int arg0 = 0;
    int arg1 = 1;
    for(int i = 0; i < tests; ++i)
    {
        green_create(&gt0, test_cond, &arg0);
        green_create(&gt1, test_cond, &arg1);
        green_join(&gt0, (void *) &arg0);
        green_join(&gt1, (void *) &arg1);

        assert(loop == 1); 
    }
}

int main(int argc, char *argv[])
{
    init();
    int tests = argc > 1 ? atoi(argv[1]) : 10000;
    printf("%d\t %d\n", argc, tests);
    printf("Testing create and join\n");
    //create_join(tests);
    printf("Done\n");

    printf("Testing mutexes\n");
    mutex(tests);
    printf("Done\n");

    printf("Testing conditional variables\n");
    cond(tests);
    printf("Done\n");

    return 0;
}