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

// WSIZE, DSIZE, CHUNKSIZE 를 정의한다
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // 2**12 = (2**3)**4이라서 항상 8의 배수이다

// MAX(x, y) 매크로를 정의한다
#define MAX(x, y) ((x) > (y)? (x) : (y))
// PACK(size, alloc) 매크로를 정의 - size와 alloc을 저장
#define PACK(size, alloc) ((size) | (alloc))
// GET(p) 매크로 정의 - 포인터 p가 참조하는 값을 출력
#define GET(p) (*(unsigned int*)(p)) // (void*)는 역참조가 불가하기 때문에 p를 int형 포인터로 받아서 역참조해주면 담긴 내용을 볼 수 있다 
// PuT(p, val) 매크로 정의 - 포인터 p가 참조하는 공간에 val을 저장
#define PUT(p, val) (*(unsigned int*)(p) = (val))
// GET_SIZE(p) 매크로 정의 - 포인터 p가 참조하는 공간에 저장된 값과와 ~0x7을 연산하여 저장해놓은 사이즈를 반환한다
#define GET_SIZE(p) (GET(p) & ~0x7)
// GET_ALLOC(p) 매크로 정의 - 포인터 p가 참조하는 공간에 저장된 값과 0x1을 연산하여 alloc 되어있으면 1 아니면 0을 반환한다
#define GET_ALLOC(p) (GET(p) & 0x1)
// 블럭을 가리키는 포인터 bp가 주어졌을 때, header와 footer의 주소를 계산하기
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// 블럭을 가리키는 포인터 bp가 주어졌을 때, 다음 블럭과 이전 블럭의 주소를 계산하기
#define NEXT_BLKP(bp) (char*)(bp) + GET_SIZE(HDRP(bp))
#define PREV_BLKP(bp) (char*)(bp) - GET_SIZE((char*)(bp)-DSIZE)

//함수 선언
static void *extend_heap(size_t words);
void mm_free(void *bp);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
void *mm_realloc(void *bp, size_t size);

void* heap_listp;

int mm_init(void)
{
    // printf("init\n");
    // 빈 힙을 초기 할당한다.
    // mem_sbrk() 에서 힙 영역을 할당받아서 할당받은 공간의 첫번째 주소를 가리키는 heap_listp를 생성하고 예외처리해준다.
    
    heap_listp = mem_sbrk(16); //힙의 최소크기 할당받아서 첫번째 주소값 반환

    if (heap_listp == (void*)-1)
    {
        return -1;
    }

    // heap_listp에 패딩, 프롤로그 헤더, 프롤로그 풋터, 에필로그 헤더를 추가해준다
    PUT(heap_listp, 0); //패딩
    PUT(heap_listp + WSIZE, PACK(8, 1));
    PUT(heap_listp + DSIZE, PACK(8, 1));
    PUT(heap_listp + DSIZE + WSIZE, PACK(0, 1)); //에필로그 헤더
    // heap_listp를 프롤로그 헤더 다음칸에 위치시킨다
    heap_listp += DSIZE;
    // chunksize만큼 빈 힙을 확장한다. //왜 chunksize/wsize 만큼을 확장하는건가.. extend_heap에서 WSIZE를 다시 곱해주긴 함
    if (extend_heap(CHUNKSIZE/DSIZE) == NULL)
    {
        return -1;
    }
    // 할당이 되면 0을 반환 안되면 -1을 반환한다
    return 0;
}
    

static void *extend_heap(size_t words)
{
    // words는 홀수만큼을 할당할 수 없다. 홀수가 들어왔을 경우 짝수로 바꿔준 후에 mem_sbrk()로 할당해준다.
    size_t size;
    char * bp;
 
    size = (words % 2)? ((words + 1) * DSIZE) : (words * DSIZE);
    if ((int)(bp = mem_sbrk(size)) == -1) // 
    {   
        return NULL; // 할당받을 수 없을 때 NULL을 반환
    }
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    // 앞 뒤에 가용 블럭이 있으면 연결해주고 리턴한다
    
    return coalesce(bp);
}
    

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
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    // 이전 블럭은 가용 블럭이고 다음 블럭은 할당되어있을 때
    
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
    }
    // 이전 블럭은 할당되어있고 다음 블럭은 가용블럭일 때
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // 이전 다음 모두 가용 블럭일 때
    else
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    }
    return bp;
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
    
//for문으로 돌면서 bp=heap_listp부터 시작해서 bp = NEXT_BLCK(bp)로 이동하면서 가용 블럭이고 size가 asize 이상인 공간을 찾아서 반환한다
static void *find_fit(size_t asize)
{
    void * bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }
    return NULL;
}


void place(void *bp, size_t asize)
{

    size_t csize = GET_SIZE(HDRP(bp));
    if (csize - asize >= 16)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
    // 찾은 공간이 asize보다 크고 남은 공간에 새로운 값이 할당될 수 있다면(16을 넘는 공간이 있다면)  나머지 공간은 가용 공간으로 처리해준다
    // 새로운 공간을 할당하기에 충분하지 않으면 할당하고 끝낸다


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