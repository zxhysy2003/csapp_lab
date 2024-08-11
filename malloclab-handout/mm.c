/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "WTF",
    /* First member's full name */
    "Yang s",
    /* First member's email address */
    "sy@sy.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* header/footer size */
#define WSIZE 4
/* double word */
#define DSIZE 8
/* header(4) + next(4) + prev(4) + payload(1) + footer(4) = 17 => 24 */
#define MIN_BLOCK 24
/* default size on extending heap */
#define CHUNKSIZE (1 << 12)

/* max function */
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* set the value of header and footer,block size + allocated bit */
#define PACK(size,alloc) ((size) | (alloc))

/* read and write the pointer position of p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (unsigned int)(val))

/* get size and allocated bit from header and footer */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* find header and footer when given payload pointer */
#define HDRP(bp) ((char *)(bp) - 3 * WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - 4 * WSIZE)

/* free block compute the field of next and pre */
#define NEXT_PTR(bp) ((char *)(bp) - 2 * WSIZE)
#define PREV_PTR(bp) ((char *)(bp) - WSIZE)

/* compute the next/pre free block address of payload */
#define NEXT_FREE_BLKP(bp) ((char *)(*(unsigned int *)(NEXT_PTR(bp))))
#define PREV_FREE_BLKP(bp) ((char *)(*(unsigned int *)(PREV_PTR(bp))))

/* find next/previous block when given payload pointer */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

static char* heap_list = NULL;
static void* allocate_from_heap(size_t size);
static void* extend_heap(size_t words);
static void insert_to_free_list(void *bp);
static void *find_fit(size_t size); //first fit
static void place(void *bp,size_t size); //split free block
static void delete_from_free_list(void *bp);
static void coalesce(void *bp);
static void *case1(void *bp);
static void *case2(void *bp);
static void *case3(void *bp);
static void *case4(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // request a block to store root pointer
    char *init_block_p = NULL;
    if((init_block_p = mem_sbrk(MIN_BLOCK + WSIZE)) == (void *)-1)
    {
        return -1;
    }
    // skip the first alignment block
    init_block_p = (char *)(init_block_p) + WSIZE;

    heap_list = init_block_p + 3 * WSIZE;
    PUT(PREV_PTR(heap_list),NULL);
    PUT(NEXT_PTR(heap_list),NULL);

    PUT(HDRP(heap_list),PACK(MIN_BLOCK,1));
    PUT(FTRP(heap_list),PACK(MIN_BLOCK,1));

    if((allocate_from_heap(CHUNKSIZE)) == NULL) {
        return -1;
    }
    return 0;
}

static void *allocate_from_heap(size_t size)
{
    void *cur_bp = NULL;
    size_t extend_size = MAX(size,CHUNKSIZE);
    if((cur_bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }

    // insert into free list
    insert_to_free_list(cur_bp);
    return cur_bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    // point to payload
    bp = (char *)(bp) + 3 * WSIZE;
    
    // set the header and footer
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));

    return bp;
}
static void insert_to_free_list(void *bp)
{
    void *head = heap_list;
    void *p = NEXT_FREE_BLKP(head);
    // LIFO strategy
    if(p == NULL)
    {
        PUT(NEXT_PTR(head),bp);
        PUT(NEXT_PTR(bp),NULL);
        PUT(PREV_PTR(bp),head);
    }
    else
    {
        PUT(NEXT_PTR(bp),p);
        PUT(PREV_PTR(bp),head);
        PUT(NEXT_PTR(head),bp);
        PUT(PREV_PTR(p),bp);
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; // adjusted block size
    char *bp;

    if(size == 0) {
        return NULL;
    }

    // step 1 : satisfy the minimal size block and alignment
    asize = ALIGN(2 * DSIZE + size); // 2 * DSIZE = header + next + prev + footer
    // step 2 : find free block from free list
    if((bp = find_fit(asize)) != NULL) {
        place(bp,asize);
    }
    else
    {
        // if not find fit block then allocate from heap
        if((bp = allocate_from_heap(asize)) == NULL) {
            return NULL;
        }
        place(bp,asize);
    }
    return bp;
}
static void *find_fit(size_t size)
{
    void *bp;

    for(bp = NEXT_FREE_BLKP(heap_list);bp != NULL && GET_SIZE(HDRP(bp)) > 0;bp = NEXT_FREE_BLKP(bp))
    {
        if(GET_ALLOC(HDRP(bp)) == 0 && size <= GET_SIZE(HDRP(bp)))
        {
            return bp;
        }
    }
    return NULL;
}
static void place(void *bp,size_t size)
{
    size_t origin_size,remain_size;

    origin_size = GET_SIZE(HDRP(bp));
    remain_size = origin_size - size;
    if(remain_size >= MIN_BLOCK)
    {
        char *remain_blockp = (char *)(bp) + size;
        PUT(HDRP(remain_blockp),PACK(remain_size,0));
        PUT(FTRP(remain_blockp),PACK(remain_size,0));
        char *pre_blockp = PREV_FREE_BLKP(bp);
        char *next_blockp = NEXT_FREE_BLKP(bp);
        PUT(PREV_PTR(remain_blockp),pre_blockp);
        PUT(NEXT_PTR(remain_blockp),next_blockp);
        PUT(NEXT_PTR(pre_blockp),remain_blockp);
        if(next_blockp != NULL) {
            PUT(PREV_PTR(next_blockp),remain_blockp);
        }

        PUT(HDRP(bp),PACK(size,1));
        PUT(FTRP(bp),PACK(size,1));

        // close the connection with free list
        PUT(NEXT_PTR(bp),NULL);
        PUT(PREV_PTR(bp),NULL);

    }
    else
    {
        PUT(HDRP(bp),PACK(origin_size,1));
        PUT(FTRP(bp),PACK(origin_size,1));

        delete_from_free_list(bp);
    }
}

static void delete_from_free_list(void *bp)
{
    void *prev_free_block = PREV_FREE_BLKP(bp);
    void *next_free_block = NEXT_FREE_BLKP(bp);

    if(next_free_block == NULL) {
        PUT(NEXT_PTR(prev_free_block),NULL);
    }
    else {
        PUT(NEXT_PTR(prev_free_block),next_free_block);
        PUT(PREV_PTR(next_free_block),prev_free_block);
    }

    PUT(PREV_PTR(bp),NULL);
    PUT(NEXT_PTR(bp),NULL);

}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);

}

static void coalesce(void *bp)
{
    char *prev_blockp = PREV_BLKP(bp);
    char *next_blockp = NEXT_BLKP(bp);
    char *mem_max_addr = (char *)mem_heap_hi() + 1;
    size_t prev_alloc = GET_ALLOC(HDRP(prev_blockp));
    size_t next_alloc;
    
    if(next_blockp >= mem_max_addr) {
        if(!prev_alloc)
        {
            case3(bp);
        }
        else
        {
            case1(bp);
        }
    }
    else {
        next_alloc = GET_ALLOC(HDRP(next_blockp));
        if(prev_alloc && next_alloc)
        {
            case1(bp);
        }
        else if(!prev_alloc && next_alloc)
        {
            case3(bp);
        }
        else if(prev_alloc && !next_alloc)
        {
            case2(bp);
        }
        else
        {
            case4(bp);
        }
    }

}
// prev_alloc && next_alloc
static void *case1(void *bp)
{
    insert_to_free_list(bp);
    return bp;
}
// prev_alloc && !next_alloc
static void *case2(void *bp)
{
    void *next_blockp = NEXT_BLKP(bp);
    void *prev_free_blockp;
    void *next_free_blockp;
    size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(next_blockp));

    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(next_blockp),PACK(size,0));

    prev_free_blockp = PREV_FREE_BLKP(next_blockp);
    next_free_blockp = NEXT_FREE_BLKP(next_blockp);

    // bound check
    if(next_free_blockp == NULL) {
        PUT(NEXT_PTR(prev_free_blockp),NULL);
    } else {
        PUT(NEXT_PTR(prev_free_blockp),next_free_blockp);
        PUT(PREV_PTR(next_free_blockp),prev_free_blockp);
    }

    insert_to_free_list(bp);
    return bp;
}
// !prev_alloc && next_alloc
static void *case3(void *bp)
{
    char *prev_blockp = PREV_BLKP(bp);
    char *prev_free_blockp;
    char *next_free_blockp;
    size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(prev_blockp));

    PUT(HDRP(prev_blockp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));

    next_free_blockp = NEXT_FREE_BLKP(prev_blockp);
    prev_free_blockp = PREV_FREE_BLKP(prev_blockp);

    // bound check
    if(next_free_blockp == NULL) {
        PUT(NEXT_PTR(prev_free_blockp),NULL);
    } else {
        PUT(NEXT_PTR(prev_free_blockp),next_free_blockp);
        PUT(PREV_PTR(next_free_blockp),prev_free_blockp);
    }

    insert_to_free_list(prev_blockp);
    return prev_blockp;
}
// !prev_alloc && !next_alloc
static void *case4(void *bp)
{
    void *prev_blockp;
    void *prev1_free_blockp;
    void *next1_free_blockp;
    void *next_blockp;
    void *prev2_free_blockp;
    void *next2_free_blockp;

    size_t size;
    prev_blockp = PREV_BLKP(bp);
    next_blockp = NEXT_BLKP(bp);

    size_t size1 = GET_SIZE(HDRP(prev_blockp));
    size_t size2 = GET_SIZE(HDRP(bp));
    size_t size3 = GET_SIZE(HDRP(next_blockp));
    size = size1 + size2 + size3;
    PUT(HDRP(prev_blockp),PACK(size,0));
    PUT(FTRP(next_blockp),PACK(size,0));


    prev1_free_blockp = PREV_FREE_BLKP(prev_blockp);
    next1_free_blockp = NEXT_FREE_BLKP(prev_blockp);
    if(next1_free_blockp == NULL) {
        PUT(NEXT_PTR(prev1_free_blockp),NULL);
    } else {
        PUT(NEXT_PTR(prev1_free_blockp),next1_free_blockp);
        PUT(PREV_PTR(next1_free_blockp),prev1_free_blockp);
    }

    prev2_free_blockp = PREV_FREE_BLKP(next_blockp);
    next2_free_blockp = NEXT_FREE_BLKP(next_blockp);
    if(next2_free_blockp == NULL) {
        PUT(NEXT_PTR(prev2_free_blockp),NULL);
    } else {
        PUT(NEXT_PTR(prev2_free_blockp),next2_free_blockp);
        PUT(PREV_PTR(next2_free_blockp),prev2_free_blockp);
    }

    insert_to_free_list(prev_blockp);
    return prev_blockp;

}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);

    if (newptr == NULL) {
        return NULL;
    }

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize) {
        copySize = size;
    }
    
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














