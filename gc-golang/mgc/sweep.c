/**
 *@ClassName sweep
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/2 0002 下午 2:07
 *@Version 1.0
 **/
#include "../sys/gc.h"
#include "../heap/heap.h"
#include "../atomic/atomic.h"
#include "../span/span.h"
#include "../sys/gpm.h"
#include "gc_sweepbuf.h"
#include "bitmap.h"


/**
 *
 * 1. 对未被标记的白色span进行回收处理，并放回heap且连接到mcentral链表上
 * 2. 对span只进行初始化工作，初始化对应位图为下次gc做准备
 * @param s
 * @param preserve
 * @return
 */
bool span_sweep(span* s,bool preserve)
{
	heap* h = &heap_;
	//当前清理工作是防止抢占的，在没有清理完之前防止进入下一轮gc
	g* _g_ = getg();
//	if( _g_->m->mallocing == 0 && _g_ != _g_->m->g0 ){
//		throw("mspan.sweep: m is not locked")
//	}
	uint32 sweepgen = h->sweepgen;
	if( s->state != mSpanInUse || s->sweepgen != sweepgen-1 ){
		throw("mspan.sweep: bad span state")
	}
	// FIXME: gc count
	// atomic_Xadd64(&h->pagesSwept, (int64)s->npages));

	spanclass spc = s->spanclass;
	uintptr size  = s->elemsize;
	bool res 	  = false;

	cache* c = _g_->m->mcache;
	bool freeToHeap = false;


	// Count the number of free objects in this span.
	uint16  nalloc = (uint16)(span_countAlloc(s));
	if( sizeclass(spc) == 0 && nalloc == 0 ){
		s->needzero = 1;
		freeToHeap = true;
	}
	uintptr nfreed = s->allocCount - nalloc;
	if( nalloc > s->allocCount ){
		throw("sweep increased allocation count")
	}

	s->allocCount = nalloc;
	bool wasempty = span_nextFreeIndex(s) == s->nelems;
	//重置分配索引 0
	s->freeindex  = 0;

	// gcmarkBits becomes the allocBits.
	// get a fresh cleared gcmarkBits in preparation for next GC
	s->allocBits  = s->gcmarkBits;
	s->gcmarkBits = newMarkBits(s->nelems);

	// 初始化位图缓存 从0 开始
	span_refillAllocCache(s,0);

	// We need to set s.sweepgen = h.sweepgen only when all blocks are swept,
	// because of the potential for a concurrent free/SetFinalizer.
	// But we need to set it before we make the span available for allocation
	// (return it to heap or mcentral), because allocation code assumes that a
	// span is already swept if available for allocation.
	if( freeToHeap || nfreed == 0 ){
		// The span must be in our exclusive ownership until we update sweepgen,
		// check for potential races.
		if( s->state != mSpanInUse || s->sweepgen != sweepgen-1 ){
			throw("mspan.sweep: bad span state after sweep")
		}
		// Serialization point.
		// At this point the mark bits are cleared and allocation ready
		// to go so release the span.
		atomic_Store(&s->sweepgen,sweepgen);
	}

	if( nfreed > 0 && sizeclass(spc) != 0 ){
		c->local_nsmallfree[sizeclass(spc)] += (uintptr)(nfreed);

		res = central_freeSpan(&h->centrals[spc],s,preserve, wasempty);
		// mcentral->freeSpan updates sweepgen
	} else if( freeToHeap ){
		heap_freeSpan(s,true);
//		c->local_nlargefree++;
//		c->local_largefree += size;
		res = true;
	}
	if( !res ){
		//当前span已经被清除过了，但span还可以继续使用，入栈继续监控
		gcsweepbuf_push(&h->sweepSpans[sweepgen/2%2],s);
	}
	return res;
}


