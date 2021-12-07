#include <stdio.h>
#include <stddef.h>
#include "dlmall.h"
int main(int argc, char* argv[])
{
    init();
    
    printf("Traversing\n");
    traverse();

    size_t request = (size_t) 500;
    int *alloc = dalloc((size_t)8);
    printf("Allocated memory at %u\n", alloc);
    
    int *size;
    flist_size(size);
    printf("flist size %u\n", *size);

    printf("Traversing again\n");
    traverse();

    printf("Freeing memory at adress %u\n", alloc);
    dfree(alloc);

    *size = 0;
    flist_size(size);
    printf("flist size %u\n", *size);

    printf("Sanity check\n");
    sanity();

    printf("Traversing for the last time!\n");
    traverse();

    int *alloc2 = dalloc((size_t)2000);

    *size = 0;
    flist_size(size);
    printf("flist size %u\n", *size);

    // int *alloc3 = dalloc((size_t) 8);
     
     int *allocArr[100];

    // for(int i = 0; i < 10; ++i)
    // {
    //      allocArr[i] = dalloc((size_t) 16);
    // }

    *size = 0;
    flist_size(size);
    printf("flist size %u\n", *size);

    dfree(alloc2);
    
    
    
    *size = 0;
    flist_size(size);
    printf("flist size %u\n", *size);
    for(int j = 0; j < 10; ++j)
    {
        dfree(allocArr[j]);
    }
    return 0;
}