/**
 *@ClassName bitmap
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/2 0002 下午 2:09
 *@Version 1.0
 **/
#include "mark.h"
#include "../sys/gc.h"
#include "../span/span.h"
#include "bitmap.h"
#include "../heap/arena.h"
#include "../heap/heap.h"
#include "bitarena.h"
#include "../atomic/atomic.h"

uint8 oneBitCount[256] = {
	0, 1, 1, 2, 1, 2, 2, 3,
	1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4,
	2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4,
	2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4,
	2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6,
	4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4,
	2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6,
	4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5,
	3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6,
	4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6,
	4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7,
	5, 6, 6, 7, 6, 7, 7, 8};


 /**
  * setMarkedNonAtomic sets the marked bit in the markbits, non-atomically.
  * @param m
  */
 void mb_setMarkedNonAtomic(markBits* m)
 {
 	*m->bytep |= m->mask;
 }

 /**
  * @param s
  * @return
  */
 markBits span_markBitsForBase(span* s)
 {
     markBits mb = {s->gcmarkBits,(uint8)(1),0};
     return mb;
 }
/**
 *
 * @param s
 * @param allocBitIndex
 * @return
 */
 markBits span_allocBitsForIndex(span* s ,uintptr allocBitIndex)
 {
     uint8* bytep = s->allocBits + (allocBitIndex/8);
     uint8  mask  = 1 << (allocBitIndex%8);
     markBits mb  = {bytep,mask,allocBitIndex};
     return mb;
 }
 // advance advances the markBits to the next object in the span.
 void mb_advance(markBits* m)
 {
 	if ( m->mask == 1<<7) {
 		m->bytep = m->bytep + 1;
 		m->mask = 1;
 	} else {
 		m->mask = m->mask << 1;
 	}
 	m->index++;
 }
 /**
  * 初始化span 的位图信息
  * @param h
  * @param s
  */
 void hb_initSpan(heapBits h,span* s)
 {
    uintptr size,n,total;
    span_layout(s,&size,&n,&total);

	// Init the markbit structures
	s->freeindex = 0;
	s->allocCache = ~((uint64)(0)); // all 1s indicating all free->
	s->nelems = n;
	s->allocBits = NULL;
	s->gcmarkBits = NULL;
	//初始化gc位图 和 分配位图
	s->gcmarkBits = newMarkBits(s->nelems);
	s->allocBits = newMarkBits(s->nelems);

	// Clear bits corresponding to objects.
	uintptr nw = total / ptrSize;
	if ( nw%wordsPerBitmapByte != 0 ){
		throw("initSpan: unaligned length");
	}
	if (h.shift != 0 ){
		throw("initSpan: unaligned base")
	}
	while( nw > 0 ){
	    uintptr  anw;
        heapBits hNext = hb_forwardOrBoundary(h,nw,&anw);
		uintptr nbyte  = anw / wordsPerBitmapByte;
		if (ptrSize == 8 && size == ptrSize) {
			uint8* bitp = h.bitp;
			for (uintptr i = 0; i < nbyte; i++ ){
				*bitp = bitPointerAll | bitScanAll;
				//移动下一个字节
				bitp++;
			}
		} else {
		    //清零
		    memset(h.bitp,0,nbyte);
		}
		h = hNext;
		nw -= anw;
	}
}
// forwardOrBoundary is like forward, but stops at boundaries between
// contiguous sections of the bitmap. It returns the number of words
// advanced over, which will be <= n.
heapBits hb_forwardOrBoundary(heapBits h,uintptr n,uintptr* nw)
{
	uintptr maxn = 4 * (((uintptr)h.last + 1) - (uintptr)h.bitp);
	if (n > maxn ){
		n = maxn;
	}
	*nw = n;
	return hb_forward(h,n);
}
/**
 *
 * @param h
 * @param n
 * @return
 */
heapBits hb_forward(heapBits h,uintptr n)
{
	n += (uintptr)h.shift / heapBitsShift;
	uintptr nbitp = (uintptr)h.bitp + n/4;
	h.shift = (uint32)(n%4) * heapBitsShift;
	if ( nbitp <= (uintptr)h.last ){
		h.bitp = (uint8*)nbitp;
		return h;
	}

	// We're in a new heap arena.
	uintptr past = nbitp - ((uintptr)h.last + 1);
	h.arena += 1 + (uint32)(past/heapArenaBitmapBytes);
	uint ai = (uint)h.arena;
	//找到对应的arena区
	array* ae = &heap_.arenas[arena_l1(ai)];
	heapArena** a = ARRAY_GET(ae,arena_l2(ai));
	if( ae->addr != NULL && *a != NULL){
        h.bitp = &(*a)->bitmap[past%heapArenaBitmapBytes];
		h.last = &(*a)->bitmap[heapArenaBitmapBytes-1];
	} else {
	    h.bitp = NULL;
	    h.last = NULL;
	}
	return h;
}
/**
 *
 * @param addr
 * @return
 */
heapBits hb_heapBitsForAddr(uintptr addr)
{
 	// 2 bits per word, 4 pairs per byte, and a mask is hard coded.
 	uint arena = arenaIndex(addr);
 	array* arr = &heap_.arenas[arena_l1(arena)];
 	heapArena** haa = ARRAY_GET(arr,arena_l2(arena));
 	// The compiler uses a load for NULL checking ha, but in this
 	// case we'll almost never hit that cache line again, so it
 	// makes more sense to do a value check.
 	if ( *haa == NULL) {
 		// addr is not in the heap. Return NULL heapBits, which
 		// we expect to crash in the caller.
 		return;
 	}
	 heapArena* ha = *haa;
 	heapBits h;
 	h.bitp = &ha->bitmap[(addr/(ptrSize*4))%heapArenaBitmapBytes];
 	h.shift = (uint32)((addr / ptrSize) & 3);
 	h.arena = (uint32)(arena);
 	h.last = &ha->bitmap[heapArenaBitmapBytes-1];
 	return h;
 }


 // newMarkBits returns a pointer to 8 byte aligned bytes
 // to be used for a span's mark bits->
 // 新建对应个数的位图
 uint8* newMarkBits(uintptr nelems)
 {
 	uintptr blocksNeeded = (uintptr)((nelems + 63) / 64);
 	//每个元素的对应一个bit位， *8 表示需要多少字节数量
 	uintptr bytesNeeded = blocksNeeded * 8;

 	//读不需要加锁
    gcBitsArena* head = atomic_Loadp(&gcBitsArenas.next);
    uint8* p = bitarena_tryAlloc(head,bytesNeeded);
    //内存足够，分配位图
 	if ( p != NULL) {
 		return p;
 	}

 	//位图不够分配了，需要加锁申请一个新的arenas区
 	lock(&gcBitsArenas.locks);
 	//在申请前再次尝试下分配是否可用
 	p = bitarena_tryAlloc(gcBitsArenas.next,bytesNeeded);
 	if( p != NULL) {
 		unlock(&gcBitsArenas.locks);
 		return p;
 	}

 	// Allocate a new arena. This may temporarily drop the lock.
 	gcBitsArena* fresh = bitarena_newArenaMayUnlock();

 	// If newArenaMayUnlock dropped the lock, another thread may
 	// have put a fresh arena on the "next" list. Try allocating
 	// from next again.
 	p = bitarena_tryAlloc(gcBitsArenas.next,bytesNeeded);
 	//新分配的bitarena 串联到全局arena区链表头部
 	if ( p != NULL) {
 		// Put fresh back on the free list.
 		fresh->next = gcBitsArenas.free;
 		gcBitsArenas.free = fresh;
 		unlock(&gcBitsArenas.locks);
 		return p;
 	}

 	// Allocate from the fresh arena. We haven't linked it in yet, so
 	// this cannot race and is guaranteed to succeed.
 	p = bitarena_tryAlloc(fresh,bytesNeeded);
 	if ( p == NULL) {
 		throw("markBits overflow")
 	}

 	// Add the fresh arena to the "next" list.
 	fresh->next = gcBitsArenas.next;
 	atomic_StorepNoWB(&gcBitsArenas.next,fresh);

 	unlock(&gcBitsArenas.locks);
 	return p;
 }
 /**
  *
  * @param x
  * @param size
  * @param dataSize
  * @param typ
  */
void heapBitsSetType(uintptr x, uintptr size, uintptr dataSize, type* typ)
{
}

void nextMarkBitArenaEpoch()
{
	lock(&gcBitsArenas.locks);
	if ( gcBitsArenas.previous != NULL ) {
		if ( gcBitsArenas.free == NULL ) {
			gcBitsArenas.free = gcBitsArenas.previous;
		} else {
			// Find end of previous arenas.
			gcBitsArena* last = gcBitsArenas.previous;
			for ( last = gcBitsArenas.previous; last->next != NULL; last = last->next ){}
			//找到最后一个
			last->next = gcBitsArenas.free;
			gcBitsArenas.free = gcBitsArenas.previous;
		}
	}
	gcBitsArenas.previous = gcBitsArenas.current;
	gcBitsArenas.current = gcBitsArenas.next;
	atomic_StorepNoWB(&gcBitsArenas.next,NULL);
	unlock(&gcBitsArenas.locks);
}

