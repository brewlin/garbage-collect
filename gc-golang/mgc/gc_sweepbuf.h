/**
 *@ClassName gc_sweepbuf
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/12 0012 下午 4:36
 *@Version 1.0
 **/
#ifndef GOC_GC_SWEEPBUF_H
#define GOC_GC_SWEEPBUF_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct
{
    mutex   spineLock;
    void*   spine;     // *[N]*gcSweepBlock, accessed atomically
    uintptr spineLen;  // Spine array length, accessed atomically
    uintptr spineCap;  // Spine array cap, accessed under lock

    uint32  index;
}gcSweepBuf;

void  gcsweepbuf_push(gcSweepBuf* b,span* s);
span* gcsweepbuf_pop(gcSweepBuf* b);

#endif //GOC_GC_SWEEPBUF_H
