/**
 *@ClassName atomic
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/2 0002 下午 1:51
 *@Version 1.0
 **/
#include "atomic.h"
#include "../sys/gc.h"
#include <stdio.h>
#include <stdlib.h>

// bool	runtime∕internal∕atomic·Cas64(uint64 *val, uint64 old, uint64 new)
// Atomically:
//	if(*val == *old){
//		*val = new;
//		return 1;
//	} else {
//		return 0;
//	}
//func Casuintptr(ptr *uintptr, old, new uintptr) bool
//bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval, ...)
bool atomic_Cas64(uint64* val,uint64 old, uint64 new)
{
    return __sync_bool_compare_and_swap(val,old,new);
}

bool atomic_Casuintptr(uintptr* ptr,uintptr old,uintptr new)
{
    return atomic_Cas64((uint64*)ptr,(uint64)old,(uint64)new);
}

void atomic_StorepNoWB(void* ptr , void* val)
{
    *(void**)ptr = val;
}
void* atomic_Loadp(void* ptr)
{
    return *(void**)ptr;
}
uintptr atomic_Xadduintptr(uintptr* ptr, uintptr delta)
{
    return atomic_Xadd64(ptr,delta);
}
uintptr atomic_Loaduintptr(uintptr* ptr){
    return *ptr;
}

/**
 * 进行位操作
 * @param ptr
 * @param val
 */
void atomic_Or8(uint8* ptr, uint8 val)
{
    __sync_fetch_and_or(ptr,val);
}
/**
 *
 * @param val
 * @param old
 * @return
 */
bool atomic_Cas(int32 *val, int32 old, int32 new)
{
    return __sync_bool_compare_and_swap(val,old,new);
}
uint64 atomic_Xadd64(uint64* ptr,int64 delta)
{
    return __sync_add_and_fetch(ptr,delta);
}
uint32 atomic_Xadd(uint32* ptr, int32 delta)
{
    return __sync_add_and_fetch(ptr,delta);
}
void atomic_Storeuintptr(uintptr* ptr, uintptr new)
{
    *ptr = new;
}
void atomic_Store(uint32* ptr,uint32 val)
{
    *ptr = val;
}
void  atomic_Store64(uint64* ptr,uint64 val)
{
    *ptr = val;
}
