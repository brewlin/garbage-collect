/**
 *@ClassName atomic
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/2 0002 下午 1:51
 *@Version 1.0
 **/
#ifndef GOC_ATOMIC_H
#define GOC_ATOMIC_H

#include "../sys/gc.h"

bool    atomic_Cas(int32 *val, int32 old, int32 new);
uint32  atomic_Xadd(uint32* ptr, int32 delta);
uint64  atomic_Xadd64(uint64* ptr,int64 delta);

void    atomic_Store(uint32* ptr,uint32 val);
void   atomic_Store64(uint64* ptr,uint64 val);
void    atomic_StorepNoWB(void* ptr , void* val);
void    atomic_Storeuintptr(uintptr* ptr, uintptr new);

void    atomic_Or8(uint8* ptr, uint8 val);
uintptr atomic_Loaduintptr(uintptr* ptr);
uintptr atomic_Xadduintptr(uintptr* ptr, uintptr delta);
void*   atomic_Loadp(void* ptr);
bool    atomic_Cas64(uint64* val,uint64 old, uint64 new);
bool    atomic_Casuintptr(uintptr* ptr,uintptr old,uintptr new);
#endif //GOC_ATOMIC_H
