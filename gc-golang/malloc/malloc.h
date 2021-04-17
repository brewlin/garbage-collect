/**
 *@ClassName malloc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 上午 11:00
 *@Version 1.0
 **/
#ifndef GOC_MALLOC_H
#define GOC_MALLOC_H

#include "../sys/gc.h"
#include "../span/span.h"

extern uintptr physPageSize;
span* largeAlloc(uintptr size, uint8 needzero, uint8 noscan);
void* mallocgc(uintptr size,type* typ,bool needzero);
void mallocinit();

#endif //GOC_MALLOC_H
