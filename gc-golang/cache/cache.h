/**
 *@ClassName cache
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 3:59
 *@Version 1.0
 **/
#ifndef GOC_CACHE_H
#define GOC_CACHE_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct mcache cache;

struct mcache {
    //可扫描的堆内字节数
    uintptr    local_scan;
    uintptr    tiny;
    uintptr    tinyoffset;
    //记录已经分配的小对象总数
    uintptr    local_tinyallocs;

    //保存了所有尺寸 指向该尺寸span的链表表头
    //当span不够的话 会从mcentral 的noempty中获取，需要加锁了
    //二维数组，一位代表各种size尺寸的对象
    span*     alloc[numSpanClasses];
    // number of frees for small objects (<=maxsmallsize)
    uintptr    local_nsmallfree[_NumSizeClasses];
    // 表示最后一次刷新本地缓存，如果不等于全局heap_.sweepgen,则说明过旧了，需要刷新后进行清除
    uint32     flushGen;
};
uintptr cache_nextFree(cache* c,spanclass spc,span** ss,bool *shouldhelpgc);
void    cache_refill(cache* c,spanclass spc);
cache*  allocmcache();
void    cache_releaseAll(cache* c);
#endif //GOC_CACHE_H
