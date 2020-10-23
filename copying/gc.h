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
    struct header *forwarding;
} Header;


typedef struct gc_heap {
    Header *slot;
    size_t size;
} GC_Heap;

#define TINY_HEAP_SIZE 0x4000
//计算指针 所占内存大小
#define PTRSIZE ((size_t) sizeof(void *))
#define HEADER_SIZE ((size_t) sizeof(Header))
//字节对齐 向上取整
#define ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
//x为一个header*，那么通过当前的对象 可以找到下一个使用的对象地址
//[ [header] x->size [header] x->size ....]
#define NEXT_HEADER(x) ((Header *)((size_t)(x+1) + x->size))
#define CURRENT_HEADER(x) ((Header *)x - 1)
#define ROOT_RANGES_LIMIT 100000
#define DEBUG(exp) exp


//执行gc 垃圾回收
void gc(void);
void gc_init(size_t req_size);
//回收内存
void gc_free(void *ptr);
//从堆缓存中申请一份内存
void * gc_malloc(size_t req_size);
//安全的获取header头，可以通过内存段来定位该header
// 因为如果ptr 刚好不在 header+1处的话 无法通过(ptr- sizeof(Header)) 来获取header地址
Header*  get_header(void *ptr);
//回收链表，挂着空闲链表
extern Header *free_list;

//方便实现，目前只支持一个堆
extern GC_Heap from;
extern GC_Heap to;



#endif