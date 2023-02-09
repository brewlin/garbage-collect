/**
 *@ClassName fixalloc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 2:05
 *@Version 1.0
 **/
#ifndef GOC_FIXALLOC_H
#define GOC_FIXALLOC_H

#include "../sys/gc.h"

typedef struct _mlink     mlink;
typedef void (*firstp)(void* arg,void* p); // 函数指针
typedef struct mpalloc    palloc;
typedef struct mfixalloc fixalloc;

struct _mlink {
    mlink* next;
};



//固定分配, fixalloc 是slab风格的分配器，用于分配固定大小的对象。 fixalloc分配的对象可以被释放，但是这个内存可能会被fixalloc pool复用， 因此它只能给相同类型的对象复用
struct mfixalloc {
    uintptr    size;

    firstp     first; // called first time p is returned
    void*      arg;

    mlink*      list;  //一个通用链表 指向下一个 空闲空余内存

    uintptr    chunk; // use uintptr instead of unsafe.Pointer to avoid write barriers
    uint32     nchunk;

    //记录在使用中的bytes
    uintptr    inuse;
    uint8      zero;  // zero allocations
};

struct mpalloc {
    void*      base;
    uintptr    off;
};

//TODO: init 全局分配器
extern palloc globalAlloc;
extern mutex  ga_lock;
extern void*  persistentChunks;

//初始化
void fixalloc_init(fixalloc* f,uintptr  size,firstp first,void* arg);
void fixalloc_free(fixalloc* f,uintptr* p);
uintptr* fixalloc_alloc(fixalloc* f);

#endif //GOC_FIXALLOC_H
