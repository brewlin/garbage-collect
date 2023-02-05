/**
 *@ClassName heap
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 上午 11:11
 *@Version 1.0
 **/
#ifndef GOC_HEAP_H
#define GOC_HEAP_H

#include "../sys/gc.h"
#include "../sys/array.h"
#include "treap.h"
#include "arena.h"
#include "../malloc/malloc.h"
#include "../malloc/fixalloc.h"
#include "../malloc/lineralloc.h"
#include "../central/central.h"
#include "../mgc/gc_sweepbuf.h"
typedef struct mheap heap;



// Main malloc heap 主堆
struct mheap {
    //线程锁
    mutex       locks;

    //下面是两个二叉树 存储的span
    treap       free; // free and non-scavenged spans 空闲和未回收的spans
    treap       scav;

    //包含了所有的span*
    array       allspans;

    //包含两个span栈
    //0 包含了已经被清除过了，但任然在使用中的span
    //1 包含了没有被清除过，但任然在使用中的span
    //每次heap_.sweepgen周期+2 后会自动更换位置
    gcSweepBuf  sweepSpans[2];
    //各种分配器
    //span* 分配器
    fixalloc    spanalloc;
    //cache* 分配器
    fixalloc    cachealloc;
    //大对象   分配器
    fixalloc    treapalloc;
    fixalloc    specialfinalizeralloc;
    fixalloc    specialprofilealloc;
    // allocator for arenaHints
    fixalloc    arenaHintAlloc;
    //仅在32位系统上才会用到？
    lineralloc arena;

    //保存了所有的arena的索引 uint8
    array       allarenas;
    //全局堆保存了所有的 arena ，并且用链表串起来
    arenaHint*  arenaHints;

    //arenas [6] *[20]*heapArena
    //存储二维heapAreana
//    heapArena** arenas;
    array     arenas[1 << arenaL1Bits];

    //并发内存分配与并发gc之间的 负载均衡
    uintptr     scavengeCredit;
    // 如果sweepgen == mheap.sweepgen - 2, span需要扫描
    // 如果sweepgen == mheap.sweepgen - 1, span正在被扫描
    // 如果sweepgen == mheap.sweepgen, span已经扫描完成，等待被使用
    // 如果sweepgen == mheap.sweepgen + 1, span已经被缓存，现在仍然缓存中，则需要扫描
    // 如果sweepgen == mheap.sweepgen + 3, span已经扫描过了，还处在缓存中
    uint32      sweepgen; // sweep generation, see comment in span.Mspan
    uint32      sweepdone;// 判断清除是否完成
    uint32      sweepers; //目前有多少span正在执行清理工作
    void**      dweepSpans;

    uint64      pagesInuse;
    //统计大对象已经申请的字节数
    uint64      largealloc; // bytes allocated for large objects
    //统计已经申请的大对象个数
    uint64      nlargealloc; // number of large object allocations

    //中央空闲链表数组 采用 2*67个空间来存储多尺寸对象，一半存储指针对象 另一半存储非指针对象
    central    centrals[numSpanClasses];

};


//全局堆管理器
extern heap heap_;

void    heap_init();
span*   heap_alloc(uintptr npage, uint8 spanclass, bool large , bool needzero);
span*   heap_alloc_m(uintptr npage,spanclass sc,bool large);
span*   heap_pickFreeSpan(uintptr npage);
span*   heap_allocSpanLocked(uintptr npage);
void    heap_freeSpanLocked(span* s,bool acctinuse,bool acctidle,int64 unusedsince);
void    heap_freeSpan(span* s , bool large);
void    heap_coalesce(span* s);
bool    heap_grow(uintptr  npage);

void    heap_setSpan(uintptr base,span* s);
void    heap_setSpans(uintptr base,uintptr npage,span* s );
span*   heap_spanOf(uintptr p);
void    heap_scavengeLargest(uintptr nbytes);
span*   heap_allocManual(uintptr npage);
#endif //GOC_HEAP_H
