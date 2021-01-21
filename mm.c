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
#include <sys/mman.h>
#include <errno.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
        /* Team name */
        "JUNGLE",
        /* First member's full name */
        "HAN JEONGSEO",
        /* First member's email address */
        "email",
        /* Second member's full name (leave blank if none) */
        "second",
        /* Second member's email address (leave blank if none) */
        "email"
};
/*
명시적 할당기 구현
가용 블럭을 연결리스트로 연결
선입후출 방식으로 연결리스트를 구현하기 때문에 First fit이 Next fit 보다 효율이 좋다.
*/

#define	WSIZE		4 //한 워드 크기
#define	DSIZE		8 //더블 워드 크기
#define CHUNKSIZE (1<<12) // 2**12 = (2**3)**4이라서 항상 8의 배수이다

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)		(*(unsigned int *)(p)) //가리키는 곳의 값을 읽어온다
#define PUT(p, val)	(*(unsigned int *)(p) = (val)) //주로 p에 값 val을 저장한다.

#define GET_SIZE(p)	(GET(p) & ~0x7) //주소 p에 있는 크기를 읽어온다.
#define GET_ALLOC(p)	(GET(p) & 0x1) //주소 p에 있는 allocated bit를 읽어온다.

#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

#define SUCC_FREEP(bp)  (*(void**)(bp + WSIZE))     // *(GET(SUCC_FREEP(bp))) == 다음 가용리스트의 bp //successor
#define PREC_FREEP(bp)  (*(void**)(bp))          // *(GET(PREC_FREEP(bp))) == 다음 가용리스트의 bp //Predecessor

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void*bp, size_t asize);
void removeBlock(void *bp);
void putFreeBlock(void *bp);

static char *heap_listp = NULL;
static char *free_listp = NULL; // 첫번째 가용블럭의 포인터로 사용

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(24); //힙의 최소크기 할당받아서 첫번째 주소값 반환

    if (heap_listp == (void*)-1)
    {
        return -1;
    }

    PUT(heap_listp, 0); //패딩
    PUT(heap_listp + WSIZE, PACK(16, 1)); //프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, NULL); //프롤로그 PREC 포인터 NULL로 초기화
    PUT(heap_listp + 3*WSIZE, NULL); //프롤로그 SUCC 포인터 NULL로 초기화
    PUT(heap_listp + 4*WSIZE, PACK(16, 1)); //프롤로그 풋터
    PUT(heap_listp + 5*WSIZE, PACK(0, 1)); //에필로그 헤더
    // heap_listp를 프롤로그 헤더 다음칸에 위치시킨다
    heap_listp += DSIZE;
    free_listp = heap_listp; // free_listp 초기화
    //<코드리뷰> 프롤로그 블록을 6칸으로 만들어 가용리스트의 시작점을 지정해준 부분이 좋은 생각이었던 것 같습니다.

    if (extend_heap(CHUNKSIZE/DSIZE) == NULL)
    {
        return -1;
    }
    // 할당이 되면 0을 반환 안되면 -1을 반환한다
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *extend_heap(size_t words)
{
    size_t size;
    char * bp;

    size = words * DSIZE;
    if ((int)(bp = mem_sbrk(size)) == -1) // 1.bp를 long형으로 바꿔주는 이유, 2.(long)과 (void*)를 비교할 수 있는지..
                                          // <코드리뷰> 주석과 달리 int형으로 캐스팅한 이유가 궁금하네요. 보통 long이나 void*를 사용해
                                          // 비교하는데.. 음.. (void*)-1이 0xffffffff로 알고 있는데 이를 int나 long이나 같은 값으로
                                          // 반환하는 것 같고... int형으로 변환해도 주소값이 오버하는 일은 없나보네요...
    {
        return NULL; // 할당받을 수 없을 때 NULL을 반환
    }
    // 새로 생긴 가용블럭의 헤더와 풋터를 만들어주고 size와 0을 저장해주고 다음 블럭의 헤더(새로운 에필로그 헤더)에 size 0과 alloc 1을 저장해준다

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 앞 뒤에 가용 블럭이 있으면 연결해주고 리턴한다
    return coalesce(bp);
}

void *mm_malloc(size_t size)
{
    void* bp;
    size_t asize;
    if (size == 0)
    {
        return NULL;
    }
    // 만약 size가 8보다 작으면 최소사이즈인 16이 되어야한다.( 4 + 4 + 8)
    if (size <= DSIZE)
    {

        asize = 2 * DSIZE;
    }
        // 8보다 크다면 올림해서 8의 배수가 되는 수 중 최소값이 되어야한다.
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //size보다 큰 8의 배수 중 최소값
    }
    // <코드리뷰> 정확히는 size에 헤더와 푸터의 공간인 8을 더한 후 그것보다 큰 8의 배수 중 최소값이 맞는 것 같습니다.
    // 예를 들어, size가 9일때, 9보다 큰 8의 배수 중 최소값은 16이지만 위의 결과는 24입니다. 헤더와 푸터의 공간 8이 추가됐기 때문이지요.

    // asize값이 들어갈 수 있는 가용 공간을 찾는다. find_fit은 bp를 반환한다

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    else
    {
        size_t extend_size;
        extend_size = MAX(asize, CHUNKSIZE);
        bp = extend_heap(extend_size/DSIZE);

        if (bp == NULL)
        {
            return NULL;
        }
        place(bp, asize);
        return bp;
    }
    // 가용공간을 할당공간으로 바꿔준다. place
    // 만약 가용 공간이 없으면 extendsize(asize, CHUNKSIZE)만큼을 힙에서 할당받고 할당받은 힙의 제일 앞쪽에 place 한다.

}

static void *find_fit(size_t asize)
{
    // first fit
    void * bp;
    bp = free_listp;

    // 프롤로그블록을 첫번째로 가용리스트에 연결시켜서 이런 종료조건을 만족할 수 있었군요. 좋은 생각인 것 같습니다.
    for (bp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCC_FREEP(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }
    return NULL;

}


static void place(void*bp, size_t asize)
{
    size_t csize;
    csize = GET_SIZE(HDRP(bp));
    // 가용리스트에서 빼주기
    removeBlock(bp);
    if (csize - asize >= 16)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        // 가용리스트 첫번째에 분할된 블럭을 넣는다 coalesce()
        putFreeBlock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


// <코드리뷰> 개인적으로 함수명이 함수의 기능을 잘 설명해 주는 것 같다고 느꼈습니다.
void putFreeBlock(void *bp)
{
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(bp) = NULL;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}

void removeBlock(void *bp)
{
    // 첫번째 블럭을 없앨 때 // 프롤로그 블럭을 지울 일을 없음
    if (bp == free_listp)
    {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
        // 앞 뒤 모두 있을 때
    else{
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);

    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // bp의 헤더에 저장되어있는 사이즈를 가지고 와서
    size_t size = GET_SIZE(HDRP(bp));
    // bp의 헤더에 저장되어있는 값을 size, 0으로 바꾸고
    PUT(HDRP(bp), PACK(size, 0));
    // bp의 풋터에 저장되어있는 값을 size, 0으로 바꿔주고
    PUT(FTRP(bp), PACK(size, 0));
    // 앞 뒤에 가용 리스트가 있을 경우가 있으니 coalesce를 해준다.
    coalesce(bp);

}

static void *coalesce(void *bp)
{
    // prev_alloc 이전 블럭이 할되어있는지 확인
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    // next_alloc 다음 블럭이 할당되어있는지 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // size bp 블럭의 사이즈 저장
    size_t size = GET_SIZE(HDRP(bp));

    // 이전 블럭, 다음 블럭이 모두 할당되어있을 때
    if (prev_alloc && next_alloc){}
    // <코드리뷰> 음.. 불필요한 라인이 있는 것 같아 아쉬운 것 같습니다.

        // 이전 블럭은 가용 블럭이고 다음 블럭은 할당되어있을 때
    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
        // 이전 블럭은 할당되어있고 다음 블럭은 가용블럭일 때
    else if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
        // 이전 다음 모두 가용 블럭일 때
    else
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    putFreeBlock(bp);

    return bp;
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size){
    if(size <= 0){ //사이즈가 0보다 작으면 free해버리기
        mm_free(bp);
        return 0;
    }
    if(bp == NULL){
        return mm_malloc(size); //bp가 NULL이면 그만큼 malloc하기
    }
    void *newp = mm_malloc(size); //새로운 pointer.
    if(newp == NULL){
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(bp));
    if(size < oldsize){
        oldsize = size; //shrink size.
    }
    memcpy(newp, bp, oldsize);
    mm_free(bp);
    return newp;
}