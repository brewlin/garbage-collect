/**
 *@ClassName central
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 3:36
 *@Version 1.0
 **/
#ifndef GOC_CENTRAL_H
#define GOC_CENTRAL_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct mcentral central;

struct mcentral {
    mutex     locks;
    uint8     spanclass; // 规格大小

    spanlist  nonempty;  // 尚有空闲object的空闲mspan链表 list of spans with a free object, ie a nonempty free list

    spanlist  empty;     // 没有空闲obect的链表，或者已经被mcache取走了list of spans with no free objects (or cached in an mcache)
    // nmalloc is the cumulative count of objects allocated from 统计所有已分配的对象
    // this mcentral, assuming all spans in mcaches are
    // fully-allocated. Written atomically, read under STW. //操作操作自动更新的，且在stw阶段会读取使用
    uint64    nmalloc;
};

void central_init(central* c,spanclass i);
span* central_cacheSpan(central* c);
bool  central_freeSpan(central* c,span* s,bool preserve,bool wasempty);
span* central_grow(central* c);
void  central_uncacheSpan(central* c ,span* s);
#endif //GOC_CENTRAL_H
