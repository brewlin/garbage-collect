/**
 *@ClassName arena
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 2:27
 *@Version 1.0
 **/
#ifndef GOC_ARENA_H
#define GOC_ARENA_H

#include "../sys/gc.h"
#include "../span/span.h"
typedef struct marenaHint arenaHint;
typedef struct mheapArena heapArena;


//每个arena 64Mb 连续空间
struct marenaHint{
    //addr 起始地址
    uintptr    addr;
    uint8      down;
    arenaHint* next;
};

//TODO: 数组相关的需要提前初始化
struct mheapArena {
    //位图， 用来优化gc标记的一块内存
    //用2mb位图 来标记 arena 64m的使用情况
    byte    bitmap[heapArenaBitmapBytes];
    //8192 个span，每个页是8kb 但是一个span可能包含多个页 span.nPages 表示总共多少个页
    span*   spans[pagesPerArena];

    //是一个位图，使用1024 * 8 bit来标记 8192个页(8192*8KB = 64MB)中哪些页正在使用中；
    //uint8
    uint8   pageInuse[pagesPerArena/8];
    //标记页 与gc相关
    uint8   pageMarks[pagesPerArena/8];
};


heapArena* pageIndexOf(uintptr p,uintptr * pageIdx,uint8* pageMask);
uint arenaIndex(uintptr p);
//linux平台的heap.arenas 一位数组容量为1 ，windows下为64
uint arena_l1(uint i);
uint arena_l2(uint i);
void* heap_sysAlloc(uintptr n,uintptr* ssize);

#endif //GOC_ARENA_H
