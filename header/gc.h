#ifndef  GC_H
#define  GC_H
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

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
/**
 * 根
 */
typedef struct root_{
    void *ptr;  //这里存储了 用户方变量地址 因为复制完成后需要替换用户态的变量地址
    void *optr;
}root;

/* marco */
#define TINY_HEAP_SIZE 4 * 1024
//计算指针 所占内存大小
#define PTRSIZE ((size_t) sizeof(void *))
#define HEADER_SIZE ((size_t) sizeof(Header))
//堆的上限
#define HEAP_LIMIT 100000
//字节对齐 向上取整
#define ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
//x为一个header*，那么通过当前的对象 可以找到下一个使用的对象地址
//[ [header] x->size [header] x->size ....]
#define NEXT_HEADER(x) ((Header *)((size_t)(x+1) + x->size))
#define CURRENT_HEADER(x) ((Header *)x - 1)

#define ROOT_RANGES_LIMIT 100000
#define DEBUG(exp) exp

/* flags */
#define FL_ALLOC 0x1
#define FL_MARK 0x2
#define FL_SET(x, f) (((Header *)x)->flags |= f)
#define FL_UNSET(x, f) (((Header *)x)->flags &= ~(f))
#define FL_TEST(x, f) (((Header *)x)->flags & f)
#define IS_MARKED(x) (FL_TEST(x, FL_ALLOC) && FL_hit = free_listTEST(x, FL_MARK))

/* public api */
void     gc(void);                     //执行gc 垃圾回收
void     gc_free(void *ptr);           //回收内存
void*    gc_malloc(size_t req_size);   //从堆缓存中申请一份内存
Header*  gc_grow(size_t req_size);     //某些算法在内存不够的时候会扩充堆
GC_Heap* is_pointer_to_heap(void *ptr);//获取指针对应的堆首地址
//安全的获取header头，可以通过内存段来定位该header
// 因为如果ptr 刚好不在 header+1处的话 无法通过(ptr- sizeof(Header)) 来获取header地址
Header*  get_header(GC_Heap *gh, void *ptr);
void     add_roots(void* o_ptr);
void     add_heaps(size_t req_size);


/* global variable */
extern Header *free_list;
extern GC_Heap gc_heaps[HEAP_LIMIT];
extern size_t gc_heaps_used;
extern int auto_gc;
extern int auto_grow;






extern root roots[ROOT_RANGES_LIMIT];
extern size_t root_used;
#endif