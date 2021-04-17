//
// Created by root on 2021/4/3.
//

#ifndef GOC_BITMAP_H
#define GOC_BITMAP_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct {
    uint8* bitp;
    uint32 shift;
    uint32 arena;
    uint8* last;
}heapBits;

heapBits hb_heapBitsForAddr(uintptr addr);
heapBits hb_forward(heapBits h,uintptr n);
heapBits hb_forwardOrBoundary(heapBits h,uintptr n,uintptr* nw);
void     hb_initSpan(heapBits h,span* s);

uint8*   newMarkBits(uintptr nelems);
void     nextMarkBitArenaEpoch();
#endif //GOC_BITMAP_H
