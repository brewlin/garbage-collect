//
// Created by root on 2021/4/3.
//

#ifndef GOC_MARK_H
#define GOC_MARK_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct {
    uint8*  bytep;
    uint8   mask;
    uintptr index;
}markBits;

//gc
markBits gc_markBitsForAddr(uintptr p);
void     gc_marknewobject(uintptr obj,uintptr size,uintptr scanSize);

//span
uintptr  span_objIndex(span* s ,uintptr p);
markBits span_markBitsForIndex(span* s,uintptr objIndex);
markBits span_markBitsForBase(span* s);
markBits span_allocBitsForIndex(span* s ,uintptr allocBitIndex);


uint8*  newMarkBits(uintptr nelems);
void    bit_heapBitsSetType(uintptr x, uintptr size, uintptr dataSize , type* typ);

//markBits
void     mb_setMarkedNonAtomic(markBits* m);
void     mb_advance(markBits* m);
#endif //GOC_MARK_H
