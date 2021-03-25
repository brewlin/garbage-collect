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
    size_t size;              //当前内存块大小
    struct header *next_free; //回收链表中使用，指向下一个空闲内存
} Header;

/**
 * 这个是一个内存池，用户层面的堆
 * 每个堆默认 1638 字节
 * 用户在申请内存时 从这1638上面分配一段地址
 * 会用在回收内存时 不用free，回收后会给其他的继续使用 (实现的机制就是上面的 header flags)
 */
typedef struct gc_heap {
    //内存开始地址
    Header *slot;
    //bit位的起始位置
    Header *bit;
    //位图大小
    int    bit_size;
    Header *end;
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
#define  ALIGN(x,a) (((x) + (a - 1)) & ~(a - 1))
#define  NEXT_HEADER(x) ((Header *)((size_t)(x+1) + (x->size- HEADER_SIZE))) //[ [header] x->size [header] x->size ....]
#define  CURRENT_HEADER(x) ((Header *)x - 1)




/* public api */
void     gc(void);                     //执行gc 垃圾回收
void     gc_free(void *ptr);           //回收内存
void*    gc_malloc(size_t req_size);   //内存分配
#define  IS_HEAP(ptr) ((((void *)heap.bit) <= ptr) && ((size_t)ptr < (size_t)heap.end))
void     gc_init();
/* global variable */
extern   Header *free_list;
extern   GC_Heap heap;


/* roots */
#define  ROOT_RANGES_LIMIT 100000
#define  DEBUG(exp) exp
extern   root roots[ROOT_RANGES_LIMIT];
extern   size_t root_used;
void     add_roots(void* o_ptr);       //模拟根，真正意义的根来自全局、栈、寄存器等、这里模拟用数组实现
#endif