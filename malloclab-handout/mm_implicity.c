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

/* default size on extending heap */
#define CHUNKSIZE (1 << 12)

/* max function */
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* set the value of header and footer,block size + allocated bit */
#define PACK(size,alloc) ((size) | (alloc))

/* read and write the pointer position of p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) ((*(unsigned int *)(p)) = (val))

/* get size and allocated bit from header and footer */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* find header and footer when given payload pointer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* find next/previous block when given payload pointer */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))


/* always point to the second prologue block */
static char *heap_list = NULL;

static void *extend_heap(size_t words);  //extend heap
static void *coalesce(void *bp); //coalesce free block
static void *find_fit(size_t asize); //first fit
// static void *best_fit(size_t asize); //best fit
static void place(void *bp,size_t asize); //split free block

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* request 4 bytes space */
    if((heap_list = mem_sbrk(4 * WSIZE)) == (void *) -1) {
        return -1;
    }
    PUT(heap_list,0);
    PUT(heap_list + (1 * WSIZE),PACK(DSIZE,1));
    PUT(heap_list + (2 * WSIZE),PACK(DSIZE,1));
    PUT(heap_list + (3 * WSIZE),PACK(0,1));
    heap_list += (2 * WSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

// extend heap
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // initialize the header,footer and epilogue header of the free block
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    // Coalesce if the previous block was free
    return coalesce(bp);

}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;

    if(size == 0) {
        return NULL;
    }

    // Adjust block size to include overhea and alignment reqs;
    if(size <= DSIZE) {
        asize = 2 * DSIZE;
    }
    else {
        // ensure asize is multiple of DSIZE and greater than size
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // Search the free list of a fit
    if((bp = find_fit(asize)) != NULL) {
        place(bp,asize);
    }
    else {
        // No fit found.Get more memory and place the block
        extendsize = MAX(asize,CHUNKSIZE);
        if((bp = extend_heap(extendsize / WSIZE)) == NULL) {
            return NULL;
        }
        place(bp,asize);
    }

    return bp;
}

// traverse the whole list to find the fit block
static void *find_fit(size_t size)
{
    void *bp;
    // complexity of time is O(n)
    for(bp = NEXT_BLKP(heap_list);GET_SIZE(HDRP(bp)) > 0;bp = NEXT_BLKP(bp)) {
        if(GET_ALLOC(HDRP(bp)) == 0 && size <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }
    return NULL;
}

// 
static void place(void *bp,size_t size)
{
    size_t remain_size;
    size_t origin_size;

    origin_size = GET_SIZE(HDRP(bp));
    remain_size = origin_size - size;
    // if the remain size >= 2 * DSIZE then split the block
    if(remain_size >= 2 * DSIZE) {
        PUT(HDRP(bp),PACK(size,1));
        PUT(FTRP(bp),PACK(size,1));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(remain_size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(remain_size,0));
    }
    // if the remain size < 2 * DSIZE then retain the inner fragment
    else {
        PUT(HDRP(bp),PACK(origin_size,1));
        PUT(FTRP(bp),PACK(origin_size,1));
    }
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

static void *coalesce(void *bp)
{
    size_t pre_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1: pre and next are both allocated
    if(pre_alloc && next_alloc) {
        return bp;
    }
    // case 2: pre allocated and next free
    else if(pre_alloc && !next_alloc) {
        void *next_block = NEXT_BLKP(bp);
        size += GET_SIZE(HDRP(next_block));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(next_block),PACK(size,0));
    }
    // case 3:pre free and next allocated
    else if(!pre_alloc && next_alloc) {
        void *pre_block = PREV_BLKP(bp);
        size += GET_SIZE(HDRP(pre_block));
        PUT(HDRP(pre_block),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
        bp = pre_block;
    }
    // case 4:pre and next are both free
    else {
        void *pre_block = PREV_BLKP(bp);
        void *next_block = NEXT_BLKP(bp);
        size += GET_SIZE(HDRP(pre_block)) + GET_SIZE(HDRP(next_block));
        PUT(HDRP(pre_block),PACK(size,0));
        PUT(FTRP(next_block),PACK(size,0));
        bp = pre_block;
    }
    
    return bp;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // request new memory space
    newptr = mm_malloc(size);
    if(newptr == NULL) {
        return NULL; 
    }
    // get the old memory size
    copySize = GET_SIZE(HDRP(oldptr));
    if(size < copySize) {
        copySize = size;
    }
    // copy the data from old mem to new mem
    memcpy(newptr,oldptr,copySize);
    // free the old mem
    mm_free(oldptr);
    return newptr;
}














