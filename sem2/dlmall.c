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
    return (struct head *)((char *)block + block->size + HEAD); // take the current pointer, cast it to a character pointer then add the size of the block plus the size of a header.
}

struct head *before(struct head *block)
{
    return (struct head *)((char *)block - block->bsize - HEAD); // same as before but now bsize bc before size and -HEAD bc previous block is before
}

//  -   -   -   -   -   -   -   -   -   --  -       --  -  split a block
struct head *split(struct head *block, int size)
{
    // Calculate the remaining size
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
    else
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

//  -   -   -   -   -   -   -   -   -   --  -       --  -     -   -   -   -   -   -   -   -   -   --  -       --  -    Allocate & free

int adjust(size_t request) // Determine a suitable size that is an even multiple of ALIGN and not smaller than the minimum size.
{
    int min = MIN(request);

    if (min % ALIGN == 0)
    {
        return request;
    }
    else
    {
        return min + ALIGN - min % ALIGN;
    }
}

// Go through freelist and find the first block which is large enough to meet our request. If there is no freelist create one.
// If the size of the block found is large enough that it can be split into two, do so.
// Mark the block as taken, and update the block after the taken block
// Return a pointer to the start of the block.
struct head *find(size_t size)
{
    struct head *traverse = flist;
    struct head *aft;

    while (traverse != NULL)
    {
        if (traverse->size >= size)
        {
            detach(traverse);

            if (traverse->size >= LIMIT(size))
            {
                struct head *splt = split(traverse, size);
                insert(traverse);

                aft = after(splt);
                aft->bfree = FALSE;
                aft->free = FALSE;

                return splt;
            }
            else
            {
                traverse->free = FALSE;
                aft = after(traverse);
                aft->bfree = FALSE;
                return traverse;
            }
        }
        else
        {
            traverse = traverse->next;
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
        
        return NULL;
    }

    return block;
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

void dfree(void *memory)
{
    if (memory != NULL)
    {
        struct head *block = MAGIC(memory);
        block = merge(block);
        struct head *aft = after(block);
        block->free = TRUE;
        aft->bfree = TRUE;
        insert(block);
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
    insert(first);
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
            //printf("Block %d:\t%d\n", i, sizes[i]);
        }
        ++i;
    }
    //printf("\n");
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