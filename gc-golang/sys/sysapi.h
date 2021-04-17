/**
 *@ClassName sysapi
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/30 0030 下午 2:50
 *@Version 1.0
 **/
#ifndef GOC_SYSAPI_H
#define GOC_SYSAPI_H

#include "gc.h"

#define AMD64 1
#define ARM   2
#define ARM64 3
#define I386  4
#define MIPS  5
#define MIPS64 6
#define PPC64 7
#define S390X 8
#define WASM  9

#define ArchFamily          AMD64
#define BigEndian           false
#define DefaultPhysPageSize 4096
#define PCQuantum           1
#define Int64Align          8
#define HugePageSize        (1 << 21)
#define MinFrameSize        0


void* sys_alloc(uintptr n);
void  sys_unused(void* v,uintptr n);
void  sys_used(void* v,uintptr n);
void  sys_free(void* v,uintptr n);
void  sys_fault(void* v,uintptr n);
void* sys_reserve(void* v,uintptr n);
void* sys_reserveAligned(void* v,uintptr* ssize,uintptr align);
void  sys_map(void* v,uintptr n);
void* sys_fixalloc(uintptr size,uintptr align);
#endif //GOC_SYSAPI_H
