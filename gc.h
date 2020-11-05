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
    size_t ref;               //引用计数中使用，其他算法忽略
    size_t flags;             //marked,remembered,copied
    size_t size;              //当前内存块大小
    size_t age;               //分代回收中使用，表示年龄
    struct header *next_free; //回收链表中使用，指向下一个空闲内存
    struct header *forwarding;//复制算法中使用, 指向拷贝后的新内存地址
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
 * 真正意义上的根可以为 全局变量、栈变量、寄存器变量等信息
 * 这里为了测试通过全局数组来模拟根
 */
typedef struct root_{
    void *ptr; //从heap中申请的内存地址
    void *optr;//用户栈变量的地址
}root;

/* marco */
#define  TINY_HEAP_SIZE 4 * 1024              //计算指针 所占内存大小
#define  PTRSIZE ((size_t) sizeof(void *))
#define  HEADER_SIZE ((size_t) sizeof(Header))//堆的上限
#define  HEAP_LIMIT 100000                    //字节对齐 向上取整
#define  ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
#define  NEXT_HEADER(x) ((Header *)((size_t)(x+1) + (x->size- HEADER_SIZE))) //[ [header] x->size [header] x->size ....]
#define  CURRENT_HEADER(x) ((Header *)x - 1)



/* flags */
#define  FL_ALLOC  0x1
#define  FL_MARK   0x2
#define  FL_COPIED 0x4
#define  FL_REMEMBERED 0x8
#define  FL_SET(x, f) (((Header *)x)->flags |= f)
#define  FL_UNSET(x, f) (((Header *)x)->flags &= ~(f))
#define  FL_TEST(x, f) (((Header *)x)->flags & f)
#define  IS_MARKED(x) (FL_TEST(x, FL_ALLOC) && FL_TEST(x, FL_MARK))
#define  IS_COPIED(x) (FL_TEST(x, FL_ALLOC) && FL_TEST(x, FL_COPIED))
#define  IS_REMEMBERED(x) (FL_TEST(x, FL_ALLOC) && FL_TEST(x, FL_REMEMBERED))

/* public api */
void     gc(void);                     //执行gc 垃圾回收
void     gc_init(size_t heap_size);    //有些算法不需要动态扩充堆，那么就需要提前初始化堆
void     gc_free(void *ptr);           //回收内存
void*    gc_malloc(size_t req_size);   //内存分配
GC_Heap* is_pointer_to_heap(void *ptr);//获取指针对应的堆首地址
GC_Heap* is_pointer_to_space(void *ptr,size_t i);
//安全的获取header头，可以通过内存段来定位该header
// 因为如果ptr 刚好不在 header+1处的话 无法通过(ptr- sizeof(Header)) 来获取header地址
Header*  get_header(GC_Heap *gh, void *ptr);
void     add_heaps(size_t req_size);   //扩充堆
Header*  gc_grow(size_t req_size);     //某些算法在内存不够的时候会扩充堆


/* global variable */
extern   Header *free_list;
extern   GC_Heap gc_heaps[HEAP_LIMIT];
extern   size_t gc_heaps_used;
extern   int auto_gc;                  //测试的时候 有时候需要关闭内存不够时执行gc
extern   int auto_grow;                //测试的时候 有时候需要关闭内存不够时扩充堆


/* roots */
#define  ROOT_RANGES_LIMIT 100000
#define  DEBUG(exp) exp
extern   root roots[ROOT_RANGES_LIMIT];
extern   size_t root_used;
void     add_roots(void* o_ptr);       //模拟根，真正意义的根来自全局、栈、寄存器等、这里模拟用数组实现
#endif