//
// Created by root on 2021/4/3.
//

#include "bitarena.h"
#include "../atomic/atomic.h"
#include "../sys/sysapi.h"
#include "../sys/gc.h"

//全局 位图 分配器
_gcBitsArenas gcBitsArenas;

uint8* bitarena_tryAlloc(gcBitsArena* b ,uintptr bytes) 
{
    //len(b->bits)
    uintptr bitslen = gcBitsChunkBytes - gcBitsHeaderBytes;
	if ( b == NULL || atomic_Loaduintptr(&b->free)+bytes > bitslen)
	{
		return NULL;
	}
	// Try to allocate from this block.
	uintptr end = atomic_Xadduintptr(&b->free, bytes);
	if ( end > bitslen) {
		return NULL;
	}
	// There was enough room.
	uintptr start = end - bytes;
	return &b->bits[start];
}

/**
 * 
 * 位图arena不够分配位图了，需要扩充arena堆
 * 这里会短暂的释放锁，所以必须调用方先持有锁
 * @return 
 */
gcBitsArena* bitarena_newArenaMayUnlock()
{
    gcBitsArena* result;
	if ( gcBitsArenas.free == NULL) {
		unlock(&gcBitsArenas.locks);
		//向操作系统申请一份
		result = sys_alloc(gcBitsChunkBytes);
		if (result == NULL ) {
			throw("runtime: cannot allocate memory")
		}
		lock(&gcBitsArenas.locks);
	} else {
		result = gcBitsArenas.free;
		gcBitsArenas.free = gcBitsArenas.free->next;
		//清零
		memset(result,0,gcBitsChunkBytes);
	}
	result->next = NULL;
	result->free = 0;
	return result;
}
