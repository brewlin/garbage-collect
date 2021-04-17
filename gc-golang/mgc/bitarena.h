//
// Created by root on 2021/4/3.
//

#ifndef GOC_BITARENA_H
#define GOC_BITARENA_H

#include "../sys/gc.h"

/**
 * type gcBitsHeader struct {
	free uintptr // free is the index into bits of the next free byte.
	next uintptr // *gcBits triggers recursive type bug. (issue 14620)
}
 */
#define gcBitsChunkBytes ((uintptr)(64 << 10))
//sizeof(gcBitsHeader{})
#define gcBitsHeaderBytes 16

typedef struct _gcBitsArena gcBitsArena;
/**
 * 专用于位图的分配器
 */
struct _gcBitsArena
{
    // gcBitsHeader // side step recursive type bug (issue 14620) by including fields by hand.
    uintptr free;// free is the index into bits of the next free byte; read/write atomically

    gcBitsArena* next;
    uint8 bits[gcBitsChunkBytes - gcBitsHeaderBytes];
};

typedef struct {
    mutex        locks;
    gcBitsArena* free;
    gcBitsArena* next;
    gcBitsArena* current;
    gcBitsArena* previous;
}_gcBitsArenas;

extern _gcBitsArenas gcBitsArenas;


uint8*       bitarena_tryAlloc(gcBitsArena* b ,uintptr bytes);
gcBitsArena* bitarena_newArenaMayUnlock();



#endif //GOC_BITARENA_H
