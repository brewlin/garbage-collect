/**
 *@ClassName gc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/12 0012 下午 1:46
 *@Version 1.0
 **/
#ifndef GOC_GC_GC_H
#define GOC_GC_GC_H

#include "../sys/gc.h"
#include "../span/span.h"

//gcstart
void    gcStart();
//多个线程只有最后一个标记线程会触发标记结束
void    gcMarkDone();
void    gcMarkTermination();
void    gcSweep();
uintptr sweepone();
void    bgsweep();
bool    span_sweep(span* s,bool preserve);

extern __thread uintptr stk_start;
extern __thread uintptr stk_end;

#endif //GOC_GC_H
