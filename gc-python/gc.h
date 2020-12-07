#ifndef PRO_LEARN_GC_H
#define PRO_LEARN_GC_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include "Hugmem.h"

#define CO_SSIZE_T_MAX INT_MAX
#define CO_SSIZE_T_MIN INT_MIN
typedef unsigned long	Py_uintptr_t;
typedef long		Py_intptr_t;
#define PyMem_MALLOC malloc
#define Py_FatalError(str)  printf(str);exit(-1)
#define PyMem_FREE   free
/*
 * 内存分配器
 * 分配策略:
 * 1. 小内存就用当前的分配器 分配block
 * 2. 超过256bytes就进行系统malloc调用
 *
 * 3. 下面有一个循环链表，多链表，加快内存分配
 * ----------------------------------------------------------------
 *    1-8                     8                       0
 *	  9-16                   16                       1
 *	 17-24                   24                       2
 *	 25-32                   32                       3
 *	 33-40                   40                       4
 *	 41-48                   48                       5
 *	 49-56                   56                       6
 *	 57-64                   64                       7
 *	 65-72                   72                       8
 *	  ...                   ...                     ...
 *	241-248                 248                      30
 *	249-256                 256                      31
 */

#define ALIGNMENT		8			//进行8字节内存对齐
#define ALIGNMENT_SHIFT		3
#define ALIGNMENT_MASK		(ALIGNMENT - 1)

//将索引转换为字节大小      (i + 1) * 8
#define INDEX2SIZE(I) (((uint)(I) + 1) << ALIGNMENT_SHIFT)


#define SMALL_REQUEST_THRESHOLD	256
#define NB_SMALL_SIZE_CLASSES	(SMALL_REQUEST_THRESHOLD / ALIGNMENT)

/*
 * 可以通过 getpagesize() 获取系统页大小
 * 为了计算不那么复杂，默认为4k大小，已经足够适配各个平台了
 */
#define SYSTEM_PAGE_SIZE	(4 * 1024)
#define SYSTEM_PAGE_SIZE_MASK	(SYSTEM_PAGE_SIZE - 1)

//1级内存区 默认的arena 为256k的大内存块
#define ARENA_SIZE		(256 << 10)	/* 256KB */
//2级内存区 默认的pool  为4k的大小
#define POOL_SIZE		SYSTEM_PAGE_SIZE	/* must be 2^N */
#define POOL_SIZE_MASK		SYSTEM_PAGE_SIZE_MASK

// 重新定义基础类型
#undef  uchar
#define uchar	unsigned char	/* assuming == 8 bits  */

#undef  uint
#define uint	unsigned int	/* assuming >= 16 bits */

#undef  ulong
#define ulong	unsigned long	/* assuming >= 32 bits */

#undef uptr
#define uptr	Py_uintptr_t

//block定义为一个uchar
typedef uchar block;

struct pool_header {
    union {
        block *_padding;
        uint count;
    } ref;	/* number of allocated blocks    */
    block *freeblock;		//指向空闲的block链表
    struct pool_header *nextpool;	/* next pool of this size class  */
    struct pool_header *prevpool;	/* previous pool       ""        */
    uint arenaindex;		/* index into arenas of base adr */
    uint szidx;			/* block size class index	 */
    uint nextoffset;		/* bytes to virgin block	 */
    uint maxnextoffset;		/* largest valid nextoffset	 */
};
typedef struct header{
    size_t flags;
    void*  data;
}Header;

typedef struct pool_header *poolp;

//记录保存了arenas1级内存区
struct arena_object {
    //从malloc分配保存的直接地址
    uptr address;

    //对address进行4k内存对齐后的首地址 不在变动
    block* first_address;

    //对address进行4k内存对齐后的首地址
    block* pool_address;
    //在arena上 可用的内存池pool的数量，usable_areanas 中会根据这个来进行
    //排序，使得分配的时候速度会更快
    uint nfreepools;

    //总pool个数，因为进行4k内存对齐后个数可能不是 256/4
    uint ntotalpools;

    //单链表指向可用的pools
    struct pool_header* freepools;

    //所有分配的arena被关联为一个双向链表
    struct arena_object* nextarena;
    struct arena_object* prevarena;
};
extern struct arena_object* arenas;
#undef  ROUNDUP
//进行8字节对齐 进行8字节4舍5入
#define ROUNDUP(x)		(((x) + ALIGNMENT_MASK) & ~ALIGNMENT_MASK)
//计算header头的对齐大小
#define POOL_OVERHEAD		ROUNDUP(sizeof(struct pool_header))

#define DUMMY_SIZE_IDX		0xffff	/* size class of newly cached pools */

//计算出i字节能够分配多少个block
#define NUMBLOCKS(I) ((uint)(POOL_SIZE - POOL_OVERHEAD) / INDEX2SIZE(I))

extern  uint maxarenas;

//多空闲链表，加开内存分配速度 基本上O(1)
#define PTA(x)	((poolp )((uchar *)&(usedpools[2*(x)]) - 2*sizeof(block *)))
#define PT(x)	PTA(x), PTA(x)
//小技巧: 在pool内分配的内存都可以通过下面立马计算出 pool_header的地址，非常高效
#define POOL_ADDR(P) ((poolp)((uptr)(P) & ~(uptr)POOL_SIZE_MASK))
//判断指针是否合法
#define Py_ADDRESS_IN_RANGE(P, POOL)			\
	((POOL)->arenaindex < maxarenas &&		\
	 (uptr)(P) - arenas[(POOL)->arenaindex].address < (uptr)ARENA_SIZE && \
	 arenas[(POOL)->arenaindex].address != 0)


/* flags */
#define  FL_ALLOC  0x1
#define  FL_MARK   0x2
#define  FL_SET(x, f) (x |= f)
#define  FL_UNSET(x, f) (x &= ~(f))
#define  FL_TEST(x, f) (x & f)
#define  IS_MARKED(x) (FL_TEST(x, FL_ALLOC) && FL_TEST(x, FL_MARK))


#define  NOT_STACK 10
#define  TRUE 1
extern   void* sp_start;

//执行内存分配逻辑
void* Malloc(size_t nbytes);
void* Realloc(void *p, size_t nbytes);
void  Free(void *p);
void  gc();
void  gc_init();
void* gc_malloc(size_t nbytes);
void* gc_realloc(void *p, size_t nbytes);
void  gc_free(void *p);
int   gc_mark(void * ptr);
struct arena_object* new_arena(void);

extern List Hugmem;




#endif //PRO_LEARN_GC_H
