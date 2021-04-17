/**
 *@ClassName lineralloc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 2:21
 *@Version 1.0
 **/
#ifndef GOC_LINERALLOC_H
#define GOC_LINERALLOC_H

#include "../sys/gc.h"

typedef struct{
    uintptr next;// next free byte
    uintptr mapped;// one byte past end of mapped space
    uintptr end;// end of reserved space
}lineralloc;
void* linearalloc_alloc(lineralloc* l,uintptr size,uintptr align);

#endif //GOC_LINERALLOC_H
