#include "green.h"
#include <stdio.h>
void *test(void *arg)
{
    int i = *(int *)arg;
    int loop = 4;
    printf("Before while\n");
    while (loop > 0)
    {
        printf("thread %d : %d\n", i, loop);
        --loop;
        green_yield();
    }
}
int main()
{
    printf("HI\n");
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;
    printf("set variables\n");
    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);
    printf("created\n");
    green_join(&g0, NULL);
    green_join(&g1, NULL);
    printf("done\n");
    return 0;
}
