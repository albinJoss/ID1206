#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include "dlmall.h"

#define TRUE 1
#define FALSE 0

#define HEAD sizeof(struct head)

#define MIN(size) (((size) > (8)) ? (size) : (8))

#define LIMIT(size) (MIN(0) + HEAD + size)

#define MAGIC(memory) ((struct head *)memory - 1)
#define HIDE(block) (void *)((struct head *)block + 1)

#define ALIGN 8

#define ARENA (64 * 1024)

//  -   -   -   -   -   -   -   -   -   --  -       --  -  operations on a block
struct head
{
    uint16_t bfree;    // 2 bytes, the status of block before
    uint16_t bsize;    // 2 bytes, the size of block before
    uint16_t free;     // 2 bytes, the status of the block
    uint16_t size;     // 2 bytes, the size (max 2^16 i.e. 64 Ki byte )
    struct head *next; // 8 bytespointer
    struct head *prev; // 8 bytespointer
};

//  -   -   -   -   -   -   -   -   -   --  -       --  -  before and after
struct head *after(struct head *block)
{
    return (struct head *)((char *)block + block->size + HEAD);
}

struct head *before(struct head *block)
{
    return (struct head *)((char *)block - block->bsize - HEAD);
}

//  -   -   -   -   -   -   -   -   -   --  -       --  -  split a block
struct head *split(struct head *block, int size)
{
    int rsize = block->size - size - HEAD;
    block->size = rsize;

    struct head *splt = after(block);
    splt->bsize = block->size;
    splt->bfree = block->free;
    splt->size = size;
    splt->free = FALSE;

    struct head *aft = after(splt);
    aft->bsize = splt->size;

    return splt;
}

//  -   -   -   -   -   -   -   -   -   --  -       --  -   a new block
struct head *arena = NULL;

struct head *new ()
{
    if (arena != NULL)
    {
        printf("one arena already allocated \n");
        return NULL;
    }

    // using mmap, but could have used sbrk
    struct head *new = mmap(NULL, ARENA,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new == MAP_FAILED)
    {
        printf("mmap failed: error %d\n", errno);
        return NULL;
    }

    /* make room for head and dummy */
    uint size = ARENA - 2 * HEAD;
    new->bfree = FALSE;
    new->bsize = 0;
    new->free = TRUE;
    new->size = size;

    struct head *sentinel = after(new);
    /* only touch the status fields */
    sentinel->bfree = new->free;
    sentinel->bsize = new->size;
    sentinel->free = FALSE;
    sentinel->size = 0;

    /* this is the only arena we have */
    arena = (struct head *)new;
    return new;
}

//  -   -   -   -   -   -   -   -   -   --  -       --  -    free list
struct head *flist;

void detach(struct head *block)
{
    if (block->next != NULL)
    {
        block->next->prev = block->prev;
    }
    if (block->prev != NULL)
    {
        block->prev->next = block->next;
    }
    if (block == flist)
    {
        flist = block->next;
    }
}

void insert(struct head *block)
{
    block->next = flist;
    block->prev = NULL;
    if (flist != NULL)
    {
        flist->prev = block;
    }
    flist = block;
}

void insert_order(struct head *block)
{
    struct head *ptr = flist;
    if (ptr == NULL)
    {
        block->prev = NULL;
        block->next = NULL;
        flist = block;
        return;
    }
    struct head *prevPtr = ptr->prev;
    while (ptr != NULL)
    {
        if (block->size <= ptr->size)
        {
            if (prevPtr != NULL)
            {
                prevPtr->next = block;
            }
            block->prev = prevPtr;
            ptr->prev = block;
            block->next = ptr;

            if (ptr == flist)
            {
                flist = block;
            }
            return;
        }
        prevPtr = ptr;
        ptr = ptr->next;
    }

    block->next = NULL;
    prevPtr->next = block;
    block->prev = prevPtr;
}

void float_up(struct head *block)
{
    struct head *next = block->next;
    if (next == NULL)
    {
        return;
    }
    if (block->size <= next->size)
    {
        return;
    }

    detach(block);

    struct head *nextPrev = next->prev;
    while ((next != NULL))
    {
        if (block->size <= next->size)
        {
            if (nextPrev != NULL)
            {
                nextPrev->next = block;
            }
            next->prev = block;
            block->next = next;
            block->prev = nextPrev;
            if (next == flist)
            {
                flist = block;
            }
            return;
        }
        nextPrev = next;
        next = next->next;
    }
    block->next = NULL;
    nextPrev->next = block;
    block->prev = nextPrev;
}

//  -   -   -   -   -   -   -   -   -   --  -       --  -     -   -   -   -   -   -   -   -   -   --  -       --  -    Allocate & free

int adjust(size_t request) // Determine a suitable size that is an even multiple of ALIGN and not smaller than the minimum size.
{
    int min = MIN(request);

    if (min % ALIGN == 0)
    {
        return min;
    }
    else
    {
        return min + ALIGN - min % ALIGN;
    }
}
//Worst fit algorithm applied
struct head *find_worst(int size)
{
    /*if(flist == NULL)
    {
        init();

    }*/

    int largest = 0;
    struct head *block = flist;
    unsigned int index = 0;
    unsigned int i = 0;
    /*while(block != NULL)
    {
        if(block->size >= size)
        {
            if(block->size > largest)
            {
                largest = block->size;
                index = i;
            }

        }
        ++i;
        block = block->next;
    }*/

    // i = 0;
    struct head *count = flist;

    while (count->next != NULL)
    {
        count = count->next;
        // ++i;
    }

    if (count->size >= size)
    {
        detach(count);

        if (count->size >= LIMIT(size))
        {
            struct head *splt = split(count, size);
            struct head *prev = before(splt);

            insert_order(prev);

            struct head *aft = after(splt);

            aft->bfree = FALSE;
            splt->free = FALSE;

            return splt;
        }
        else
        {
            count->free = FALSE;
            struct head *aft = after(count);
            aft->bfree = FALSE;

            return count;
        }
    }

    return NULL;
}

//Best fit algorithm applied
struct head *find_best(int size)
{
    /*if (flist == NULL)
    {
        init();
    }*/

    unsigned int bestFit = UINT16_MAX;
    struct head *block = flist;
    unsigned int index = 0;
    unsigned int i = 0;

    while(block != NULL)
    {
        if(block->size >= size)
        {
            if(block->size - size < bestFit)
            {
                bestFit = block->size;
                index = i;
            }

        }
        ++i;
        
            block = block->next;
    }

    i = 0;
    struct head *count = flist;

    while (i < index && count != NULL)
    {
        count = count->next;
        ++i;
    }

    if (count->size >= size)
    {
        detach(count);

        if (count->size >= LIMIT(size))
        {
            struct head *splt = split(count, size);
            struct head *prev = before(splt);

            insert_order(prev);

            struct head *aft = after(splt);

            aft->bfree = FALSE;
            splt->free = FALSE;

            return splt;
        }
        else
        {
            count->free = FALSE;
            struct head *aft = after(count);
            aft->bfree = FALSE;

            return count;
        }
    }

    return NULL;
}

struct head *find(int size)
{
    

    struct head *next = flist;
    while (next != NULL)
    {
        if (next->size >= size)
        {
            detach(next);
            if (next->size >= LIMIT(size))
            { 
                struct head *block = split(next, size);
                struct head *bef = before(block);
                insert_order(bef); // insert(bef);
                struct head *aft = after(block);
                aft->bfree = FALSE;
                block->free = FALSE;
                return block;
            }
            else
            { 
                next->free = FALSE;
                struct head *aft = after(next);
                aft->bfree = FALSE;
                return next;
            }
        }
        else
        {
            next = next->next;
        }
    }
    return NULL; 
}

void *dalloc(size_t request)
{
    if (request <= 0)
    {
        return NULL;
    }
    int size = adjust(request);
    struct head *taken = find(size);
    if (taken == NULL)
    {
        return NULL;
    }
    else
    {
        return HIDE(taken);
    }
}

struct head *merge(struct head *block)
{
    struct head *aft = after(block);
    if (block->bfree)
    {
        struct head *bef = before(block);
        detach(bef);
        bef->size = bef->size + block->size + HEAD;
        aft->bsize = bef->size;
        block = bef;
    }

    if (aft->free)
    {
        detach(aft);
        block->size = block->size + aft->size + HEAD;
        aft = after(block);
        aft->bsize = block->size;
    }

    return block;
}

struct head *merge_no_detach(struct head *block)
{
    struct head *aft = after(block);

    if (block->bfree)
    {
        struct head *bef = before(block);
        bef->size = bef->size + block->size + HEAD;
        aft->bsize = bef->size;
        aft->bfree = bef->free;
        block = bef;

        if (aft->free)
        {
            block->size = block->size + aft->size + HEAD;
            detach(aft);
            if (flist == NULL)
            {
                flist = block;
            }
            aft = after(aft);
            aft->bsize = block->size;
            aft->bfree = block->free;
        }
        float_up(block); // keep flist ordered
        return NULL;
    }
    // only block after is free
    if (aft->free)
    {
        block->next = aft->next;
        block->prev = aft->prev;
        struct head *aftprev = aft->prev;
        struct head *aftnext = aft->next;
        if (aftprev != NULL)
        {
            aftprev->next = block;
        }
        if (aftnext != NULL)
        {
            aftnext->prev = block;
        }
        if (aft == flist)
        {
            flist = block;
        }
        block->size = block->size + aft->size + HEAD;
        block->free = TRUE;
        aft = after(block);
        aft->bsize = block->size;
        aft->bfree = block->free;
        float_up(block); // keep flist ordered
        return NULL;
    }

    return block;
}

void dfree(void *memory)
{
    if (memory != NULL)
    {
        struct head *block = MAGIC(memory);
        block = merge_no_detach(block);

        if (block == NULL)
        {
            // merged into flist already, no need to do more...
        }
        else
        {
            block->free = TRUE;
            struct head *aft = after(block);
            aft->bfree = TRUE;
            insert_order(block); // insert(block);
        }

        /*struct head *aft = after(block);
        block->free = TRUE;
        aft->bfree = TRUE;
        insert(block);*/
        /*struct head *aft = after(block);
        block->free = TRUE;
        aft->bfree = TRUE;
        insert(block);*/
    }
    return;
}

void sanity()
{
    struct head *sanityCheck = flist;
    struct head *prevCheck = sanityCheck->prev;

    while (sanityCheck->size != 0 && sanityCheck->next != NULL)
    {
        if (sanityCheck->free != TRUE)
        {
            printf("Block at index  in the list was found but was not free\n");
            exit(1);
        }

        if (sanityCheck->size % ALIGN != 0)
        {
            printf("Block at index  in the list had a size which does not align\n");
            exit(1);
        }

        if (sanityCheck->size < MIN(sanityCheck->size))
        {
            printf("The size of the block at index in the list is smaller than the mininmum.\n");
            exit(1);
        }

        if (sanityCheck->prev != prevCheck)
        {
            printf("Block at index in the list had a prev that didn't match with the previous block.\n");
            exit(1);
        }

        prevCheck = sanityCheck;
        sanityCheck = sanityCheck->next;
    }

    printf("No problems were found during the sanity check\n");
}

void traverse()
{
    struct head *before = arena;
    struct head *aft = after(before);

    while (aft->size != 0)
    {
        // printf("%u\n", aft->size);
        if (aft->bsize != before->size)
        {
            printf("the size doesn't match!\n");
            exit(1);
        }

        if (aft->bfree != before->free)
        {
            printf("the status of free doesn't match!\n");
        }

        before = aft;
        aft = after(aft);
    }
    printf("No problems deteced.\n");
}

void init()
{
    struct head *first = new ();
    insert_order(first);
}

void block_sizes(int max)
{
    unsigned int sizes[max];
    struct head *block = flist;
    int i = 0;

    while (i < max)
    {
        if (block != NULL)
        {
            sizes[i] = (int)block->size;
            block = block->next;
            // printf("Block %d:\t%d\n", i, sizes[i]);
        }
        ++i;
    }
    // printf("\n");
}

int flist_size(unsigned int *size)
{

    int count = 0;
    *size = 0;
    struct head *block = flist;

    while (block != NULL && block != block->prev)
    {
        ++count;
        *size += block->size;

        // printf("Flist: Block %d:\t%d\n", count, block->size);
        block = block->next;
    }

    return count;
}

void terminate()
{
  munmap(arena, ARENA);
  arena = NULL;
  flist = NULL;
}