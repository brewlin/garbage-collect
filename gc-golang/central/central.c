/**
 *@ClassName central
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/2 0002 上午 11:34
 *@Version 1.0
 **/
#include "central.h"
#include "../span/span.h"
#include "../heap/heap.h"
#include "../atomic/atomic.h"
#include "../mgc/bitmap.h"
#include "../mgc/gc.h"

/**
 *
 * 初始化中央空闲链表, 对应: func (c *mcentral) init
 * @param c
 * @param i
 */
void central_init(central* c,spanclass i)
{
	c->spanclass = i;
	c->empty.first = NULL;
	c->empty.last  = NULL;

	c->nonempty.first = NULL;
	c->nonempty.last  = NULL;

}

/**
  * mcahe 不够用了申请一个 msapn 给 mcache 使用
  * @param c
  * @return
  */
span* central_cacheSpan(central* c)
{
	// Deduct credit for this span allocation and sweep if necessary.
	uintptr spanBytes = (uintptr)class_to_allocnpages[sizeclass(c->spanclass)] * _PageSize;

	lock(&c->locks);
	bool traceDone = false;
	uint32 sg = heap_.sweepgen;
	
retry:;
	span* s;
	//看看nonempty 上有么有空余的span可以使用
	for ( s = c->nonempty.first; s != NULL; s = s->next )
	{
	    //如果当前span需要被清除，则马上执行清除流程
		if( s->sweepgen == sg-2 && atomic_Cas(&s->sweepgen,sg-2,sg-1))
		{
			//可以回收了
			//获取空闲的mspan成功
		    spanlist_remove(&c->nonempty,s);
			//将这个msapn加入已分配链表中
			spanlist_insertback(&c->empty,s);
			unlock(&c->locks);
			//这个地方应该是初始化该mspan的样子,参数为false 就要归还给中央列表了
			span_sweep(s,true);
			goto havespan;							 //分配完毕
		}
		if( s->sweepgen == sg-1 ){
			// the span is being swept by background sweeper, skip //这个span正在被后台回收 跳过这个
			continue;
		}
		// we have a nonempty span that does not require sweeping, allocate from it
		spanlist_remove(&c->nonempty,s);
		spanlist_insertback(&c->empty,s);
		unlock(&c->locks);
		goto havespan;
	}

	for ( s = c->empty.first; s != NULL; s = s->next ){ //上面没有找到就会继续寻找`empty`链表
		if( s->sweepgen == sg-2 && atomic_Cas(&s->sweepgen, sg-2, sg-1) ){
			// we have an empty span that requires sweeping,
			// sweep it and see if we can free some space in it
			spanlist_remove(&c->empty,s);
			// swept spans are at the end of the list
			spanlist_insertback(&c->empty,s);
			unlock(&c->locks);
			span_sweep(s,true);
			uintptr freeIndex = span_nextFreeIndex(s);
			if( freeIndex != s->nelems ){
				s->freeindex = freeIndex;
				goto havespan;
			}
			lock(&c->locks);
			// the span is still empty after sweep
			// it is already in the empty list, so just retry
			goto retry;
		}
		if( s->sweepgen == sg-1 ){ // 正在被回收 跳过
			// the span is being swept by background sweeper, skip
			continue;
		}
		// already swept empty span,
		// all subsequent ones must also be either swept or in process of sweeping
		break;
	}
	unlock(&c->locks);

	// Replenish central list if empty. //说明中央列表为空 需要补充了
	s = central_grow(c);
	if( s == NULL ){
		return NULL;
	}
	lock(&c->locks);
	//将从heap申请的span存入empty链表中
	spanlist_insertback(&c->empty,s);
	unlock(&c->locks);

havespan:;

	int n = (int)(s->nelems) - (int)(s->allocCount);
	if( n == 0 || s->freeindex == s->nelems || (uintptr)s->allocCount == s->nelems ){
		throw("span has no free objects");
	}
	// Assume all objects from this span will be allocated in the
	// mcache-> If it gets uncached, we'll adjust this.
	atomic_Xadd64(&c->nmalloc, n);
	uintptr usedBytes = (uintptr)s->allocCount * s->elemsize;
	if( gcBlackenEnabled != 0 ){
		// heap_live changed.
		//		gcController->revise();
	}
	uintptr  freeByteBase = s->freeindex &~ (64 - 1);
	uintptr  whichByte    = freeByteBase / 8;
	// Init alloc bits cache.
	//初始化 位图缓存
	span_refillAllocCache(s,whichByte);

	s->allocCache >>= s->freeindex % 64;

	return s;
}


// grow allocates a new empty span from the heap and initializes it for c's size class. 从堆申请一块mspan 并初始化话切分c'size 大小
/**
 * 中央层也不够用了需要扩充
 * @param c
 * @return
 */
span* central_grow(central* c)
{
//	printf("central not enough!\n");
	uintptr npages = (uintptr)class_to_allocnpages[sizeclass(c->spanclass)];
	uintptr size   = (uintptr)class_to_size[sizeclass(c->spanclass)];
	uintptr n 	   = (npages << _PageShift) / size;

	span* s = heap_alloc(npages, c->spanclass, false, true);
	if( s == NULL ){
		return NULL;
	}

	uintptr p = s->startaddr;
	s->limit = p + size*n;
	//新分配的span需要 分配位图、初始化位图等任务
	heapBits h = hb_heapBitsForAddr(s->startaddr);
	hb_initSpan(h,s);
	return s;
}

// span_sweep() 过后需要执行当前方法
// 1. 根据span的对象数量插入c到合适的位置方便分配
// 2. 判断是否需要归还给heap
// 3. 返回是否需要归还给堆
/**
 * @param s
 * @param preserve
 * @param wasempty
 * @return
 */
bool central_freeSpan(central* c,span* s ,bool preserve,bool wasempty)
{
    uint32 sg = heap_.sweepgen;

	if ( s->sweepgen == sg+1 || s->sweepgen == sg+3 ){
		throw("freeSpan given cached span")
	}
	s->needzero = 1;

	if (preserve ){
		// preserve is set only when called from (un)cacheSpan above,
		// the span must be in the empty list.
		if (s->list == NULL ){
			throw("can't preserve unlinked span")
		}
		atomic_Store(&s->sweepgen,heap_.sweepgen);
		return false;
	}

	lock(&c->locks);

	// Move to nonempty if necessary.
	if (wasempty ){
	    spanlist_remove(&c->empty,s);
	    spanlist_insert(&c->nonempty,s);
	}

	// delay updating sweepgen until here. This is the signal that
	// the span may be used in an mcache, so it must come after the
	// linked list operations above (actually, just after the
	// lock of c above.)
	atomic_Store(&s->sweepgen,heap_.sweepgen);

	if (s->allocCount != 0 ){
		unlock(&c->locks);
		return false;
	}
    spanlist_remove(&c->nonempty,s);
	unlock(&c->locks);
	heap_freeSpan(s,false);
	return true;
}

/**
 * 将span 归还到中央链表
 * @param s
 */
void central_uncacheSpan(central* c ,span* s)
{
	if ( s->allocCount == 0 ){
		throw("uncaching span but s.allocCount == 0")
	}

	uint32 sg  = heap_.sweepgen;
	bool stale = s->sweepgen == sg+1;
	if ( stale ){
		// Span was cached before sweep began. It's our
		// responsibility to sweep it.
		//
		// Set sweepgen to indicate it's not cached but needs
		// sweeping and can't be allocated from. sweep will
		// set s.sweepgen to indicate s is swept.
		atomic_Store(&s->sweepgen, sg-1);
	} else {
		// Indicate that s is no longer cached.
		atomic_Store(&s->sweepgen, sg);
	}

	int n = s->nelems - s->allocCount;
	if ( n > 0 ){
		// cacheSpan updated alloc assuming all objects on s
		// were going to be allocated. Adjust for any that
		// weren't. We must do this before potentially
		// sweeping the span.
//		atomic_Xadd64(&c->nmalloc, -int64(n))

		lock(&c->locks);
		spanlist_remove(&c->empty,s);
		spanlist_insert(&c->nonempty,s);
		if ( !stale ){
			// mCentral_CacheSpan conservatively counted
			// unallocated slots in heap_live. Undo this.
			//
			// If this span was cached before sweep, then
			// heap_live was totally recomputed since
			// caching this span, so we don't do this for
			// stale spans.
//			atomic_Xadd64(&memstats.heap_live, -int64(n)*int64(s.elemsize))
		}
		unlock(&c->locks);
	}

	if (stale ){
		// Now that s is in the right mcentral list, we can
		// sweep it.
		span_sweep(s,false);
	}
}