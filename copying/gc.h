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


#define TINY_HEAP_SIZE 0x4000
//计算指针 所占内存大小
#define PTRSIZE ((size_t) sizeof(void *))
#define HEADER_SIZE ((size_t) sizeof(Header))
//字节对齐 向上取整
#define ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
//x为一个header*，那么通过当前的对象 可以找到下一个使用的对象地址
//[ [header] x->size [header] x->size ....]
#define NEXT_HEADER(x) ((Header *)((size_t)(x+1) + x->size))
#define ROOT_RANGES_LIMIT 100000
#define DEBUG(exp) exp


void gc_init();
//回收内存
void gc_free(void *ptr);
//从堆缓存中申请一份内存
void * gc_malloc(size_t req_size);
//执行gc 垃圾回收
void gc(void);
//将申请的内存 加入 root 管理
void add_roots(void * start, void * end);
//进行标记
void gc_copy_range(void *start, void *end);

//定位内存实际所在的堆
//如果没有找到，说明该内存非 堆内存池中申请的内存
GC_Heap* is_pointer_to_heap(void *ptr);
//安全的获取header头，可以通过内存段来定位该header
// 因为如果ptr 刚好不在 header+1处的话 无法通过(ptr- sizeof(Header)) 来获取header地址
Header*  get_header(GC_Heap *gh, void *ptr);

void gc(void);
//回收链表，挂着空闲链表
extern Header *free_list;

//方便实现，目前只支持一个堆
extern Header* from;
extern Header* to;
//每次gc的时候将 free指向 to的开头
extern Header* free;


/****** 标记清除法实现-------------*/
typedef struct root_range {
    void *start;
    void *end;
}root;
extern root roots[ROOT_RANGES_LIMIT];
extern size_t root_used;
#endif