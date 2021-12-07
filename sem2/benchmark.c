#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include "dlmall.h"

unsigned long long cpumSecond() {
   struct timeval tp;
   gettimeofday(&tp,NULL);
   return ((double)tp.tv_sec * 1000000 + (double)tp.tv_usec);
}

unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

void benchmark(int howMany, int requested, unsigned int min, unsigned int max, FILE *fptr)
{
    unsigned long long iStart, iElaps;
    iStart = cpumSecond();
    unsigned int count = 0;
    struct head *arr[howMany];
    int j = 0;
    int flistSize = 0;
    unsigned int size;
    for (int reset = 0; reset < howMany; ++reset)
    {
        arr[reset] = NULL;
    }
    for (int i = 0; i < howMany; ++i)
    {
        int random = rand() % requested;
        int index = random == 0 ? 0 : random - 1;
        if ((arr[index]) != NULL || j >= requested)
        {
            //printf("Remove!\n");
            dfree(arr[index]);
            arr[index] = NULL;
            --j;
        }
        else
        {
            
            size = rand_interval(min, max);
            //printf("Add!\t Allocated %d bytes\n", size);
            arr[index] = dalloc((size_t)size);
            ++j;
        }
    }
    count = flist_size(&flistSize);
    block_sizes(count);
    iElaps = cpumSecond() - iStart;
    fprintf(fptr, "%llu;%d;%d\n", iElaps, flistSize, count);
}


int main(int argc, char *argv[])
{
    srand(912);
    FILE *fptr;
    fptr = fopen("numbers.txt", "w+");
    unsigned int requested = argc < 2 ? 100 : atoi(argv[1]);
    unsigned int howMany = argc < 3 ? 500 : atoi(argv[2]);
    unsigned int min = argc < 4 ? 16 : atoi(argv[3]);
    unsigned int max = argc < 5 ? 40 : atoi(argv[4]);
    printf("%d  %d\n", requested, howMany);
    //unsigned long long iStart, iElaps;
    for(int i = 100; i < 5001; i += 10)
    {
        init();
        //iStart = cpumSecond();
        benchmark(i, i / 5, min, max, fptr);
        //iElaps = cpumSecond() - iStart;
        //printf("Time: %llu\n", iElaps);
        terminate();
    }
    fclose(fptr);
    return 0;
}