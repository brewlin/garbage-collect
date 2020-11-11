#ifndef  GC_H
#define  GC_H
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <time.h>



/**
 * header头 每个用户用户申请的内存都有一个隐形的头部
 * 例如: gc_alloc(16) 实际申请了 16 + sizeof(header)
 * 那么返回给用户的地址其实是 ptr + sizeof(header)
 * 同样的也可以通过 ptr-sizeof(header) 拿到header头
 */
typedef struct header {
    size_t flags;
    size_t size;
    struct header *next_free;
} Header;

/**
 * 这个是一个内存池，用户层面的堆
 * 每个堆默认 1638 字节
 * 用户在申请内存时 从这1638上面分配一段地址
 * 会用在回收内存时 不用free，回收后会给其他的继续使用 (实现的机制就是上面的 header flags)
 */
typedef struct gc_heap {
    Header *slot;
    size_t size;
} GC_Heap;

#define TINY_HEAP_SIZE 0x4000
//计算指针 所占内存大小
#define PTRSIZE ((size_t) sizeof(void *))
#define ALIGNMENT		    8
#define ALIGNMENT_SHIFT		3

#define HEADER_SIZE ((size_t) sizeof(Header))
//堆的上限
#define HEAP_LIMIT 1000000
//字节对齐 向上取整
#define ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
//x为一个header*，那么通过当前的对象 可以找到下一个使用的对象地址
//[ [header] x->size [header] x->size ....]
#define NEXT_HEADER(x) ((Header *)((size_t)(x+1) + x->size))
#define CURRENT_HEADER(x) ((Header *)x - 1)


/* flags */
#define FL_ALLOC 0x1
#define FL_MARK 0x2
#define FL_SET(x, f) (((Header *)x)->flags |= f)
#define FL_UNSET(x, f) (((Header *)x)->flags &= ~(f))
#define FL_TEST(x, f) (((Header *)x)->flags & f)
#define IS_MARKED(x) (FL_TEST(x, FL_ALLOC) && FL_TEST(x, FL_MARK))

#define ROOT_RANGES_LIMIT 100000

#define DEBUG(exp)


//回收内存
void gc_free(void *ptr);
//从堆缓存中申请一份内存
void * gc_malloc(size_t req_size);
//执行gc 垃圾回收
void gc(void);

//定位内存实际所在的堆
//如果没有找到，说明该内存非 堆内存池中申请的内存
GC_Heap* is_pointer_to_heap(void *ptr);
//安全的获取header头，可以通过内存段来定位该header
// 因为如果ptr 刚好不在 header+1处的话 无法通过(ptr- sizeof(Header)) 来获取header地址
Header*  get_header(GC_Heap *gh, void *ptr);

void gc(void);
//回收链表，挂着空闲链表
extern Header *free_list[33];
extern GC_Heap gc_heaps[HEAP_LIMIT];
extern size_t gc_heaps_used;
extern int auto_gc;

#define MAX_SLICE_HEAP 31
#define HUGE_BLOCK     32

/****** 标记清除法实现-------------*/
void gc_mark(void * ptr);
void  gc_mark_range(void *start, void *end);
void     gc_sweep(void);
void     add_roots(void * obj);
typedef struct root_range {
    void *start;
    void *end;
}root;
extern root roots[ROOT_RANGES_LIMIT];
extern size_t root_used;
#endif