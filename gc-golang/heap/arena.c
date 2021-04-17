#include "arena.h"
#include "heap.h"
#include "../sys/gc.h"
#include "../malloc/lineralloc.h"
#include "../sys/sysapi.h"
#include "../atomic/atomic.h"


uint arenaIndex(uintptr p)
{
    return (uint)((p + arenaBaseOffset) / heapArenaBytes);
}
uint arena_l1(uint i){
    if(arenaL1Bits == 0) return 0;
    return i >> arenaL1Shift;
}
uint arena_l2(uint i){
    if(arenaL1Bits == 0) return i;
    return i & (1 << arenaL2Bits - 1);
}

// pageIndexOf returns the arena, page index, and page mask for pointer p.
// The caller must ensure p is in the heap.
heapArena* pageIndexOf(uintptr p,uintptr * pageIdx,uint8* pageMask)
{
    uint ai = arenaIndex(p);
    //获取对应的 heap堆
    array* arr = &heap_.arenas[arena_l1(ai)];
    heapArena** arena = ARRAY_GET(arr,arena_l2(ai));
    *pageIdx = ((p / pageSize) / 8) % (uintptr)(pagesPerArena/8);
    *pageMask = (byte)(1 << ((p / pageSize) % 8));
    return *arena;
}
/**
 * 申请一份 heap area 大内存块
 * 也可以叫做 arean alloc
 * @param n
 * @param ssize 返回申请了多少字节
 * @return
 */
void* heap_sysAlloc(uintptr n,uintptr* ssize)
{
    uintptr size;
    heap*   h  = &heap_;
	n = round(n, heapArenaBytes);//进行字节对齐操作

	// First, try the arena pre-reservation.
	void* v = linearalloc_alloc(&h->arena,n,heapArenaBytes);
	if( v != NULL ){
		size = n;
		goto mapped;
	}

	// Try to grow the heap at a hint address.
	while( h->arenaHints != NULL ){
		arenaHint* hint = h->arenaHints;
		uintptr p = hint->addr;
		if( hint->down ){
			p -= n;
		}
		if( p+n < p ){
			// We can't use this, so don't ask.
			v = NULL;
		} else if( arenaIndex(p+n-1) >= 1<<arenaBits ){
			// Outside addressable heap. Can't use.
			v = NULL;
		} else {
			v = sys_reserve(p, n);
		}
		if( p == (uintptr)(v) ){
			// Success. Update the hint->
			if( !hint->down ){
				p += n;
			}
			hint->addr = p;
			size = n;
			break;
		}
		if( v != NULL ){
			sys_free(v, n);
		}
		h->arenaHints = hint->next;
		fixalloc_free(&h->arenaHintAlloc,hint);
	}

	if( size == 0 ){
		// All of the hints failed, so we'll take any
		// (sufficiently aligned) address the kernel will give
		// us.
		void* v;
		uintptr  size = n;

		v = sys_reserveAligned(NULL,&size,heapArenaBytes);
		if( v == NULL ){
			*ssize = 0;
			return NULL;
		}

		// Create new hints for extending this region.
		arenaHint* hint = fixalloc_alloc(&h->arenaHintAlloc);
		hint->addr = v;
		hint->down = true;
		//将当前的arena插入链表头
		hint->next = h->arenaHints;
		h->arenaHints = hint;

		hint = fixalloc_alloc(&h->arenaHintAlloc);
		hint->addr = v + size;
		hint->down = true;
		//将当前的arena插入链表头
		hint->next = h->arenaHints;
		h->arenaHints = hint;
	}

	// Check for bad pointers or pointers we can't use.
	{
		char* bad = "";
		uintptr p = v;
		if( p+size < p ){
			bad = "region exceeds uintptr range";
		} else if( arenaIndex(p) >= 1<<arenaBits ){
			bad = "base outside usable address space";
		} else if( arenaIndex(p+size-1) >= 1<<arenaBits ){
			bad = "end outside usable address space";
		}
		if( *bad != '\0' ){
			// This should be impossible on most architectures,
			// but it would be really confusing to debug.
//			pr(int)("runtime: memory allocated by OS [", hex(p), ", ", hex(p+size), ") not in usable address space: ", bad, "\n")
			throw("memory reservation exceeds address space limit")
		}
	}

	if( (uintptr)(v)&(heapArenaBytes-1) != 0 ) {
		throw("misrounded allocation in sysAlloc")
	}

	// Back the reservation.
	sys_map(v,size);

mapped: //管理刚刚申请的空间 创建一个新的arena来管理
	// Create arena metadata.
	for ( uint ri = arenaIndex((uintptr)(v)); ri <= arenaIndex((uintptr)(v)+size-1); ri++ ) {
        //定位到一维数组的位置
		array* l2 = &h->arenas[arena_l1(ri)];
		if( l2->addr == NULL ){
			// Allocate an L2 arena map.
			//直接向操作系统分配一块大 arena内存块
			//分配一个 heapArea** 数组
			array_init(l2,1 << arenaL2Bits,ptrSize);
			if( l2->addr == NULL ){
				throw("out of memory allocating heap arena map")
			}
		}
		heapArena** initHa = ARRAY_GET(l2,arena_l2(ri));
		if( *initHa != NULL ){
			throw("arena already initialized")
		}
		heapArena* r;
		//申请一个 heapaRrena 结构体用于管理上面申请的 64M area 大内存
        r = (heapArena*)(sys_fixalloc(sizeof(heapArena), ptrSize));
        if( r == NULL ){
            throw("out of memory allocating heap arena metadata")
        }


		// Add the arena to the arenas list.
		uint8* insert = array_push(&h->allarenas);
        *insert = ri;

        //add new area to areas
        *initHa = r;
	}

    *ssize = size;
	return v;
}

