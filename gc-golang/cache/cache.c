/**
 *@ClassName cache
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/1 0001 下午 5:33
 *@Version 1.0
 **/
#include "cache.h"
#include "../span/span.h"
#include "../heap/heap.h"
#include "../atomic/atomic.h"

span emptyspan;

/**
 *
 * @param c
 * @param spc
 * @param ss
 * @param shouldhelpgc
 * @return
 */
uintptr cache_nextFree(cache* c,spanclass spc,span** ss,bool* shouldhelpgc)
{

	span* s = c->alloc[spc];
	*shouldhelpgc = false;
	uintptr freeIndex = span_nextFreeIndex(s);

	if( freeIndex == s->nelems ){
		// The span is full.
		if( (uintptr)s->allocCount != s->nelems ){
			throw("s.allocCount != s.nelems && freeIndex == s.nelems")
		}
		cache_refill(c,spc);
		*shouldhelpgc = true;
		s = c->alloc[spc];

		freeIndex = span_nextFreeIndex(s);
	}

	if( freeIndex >= s->nelems ){
		throw("freeIndex is not valid")
	}
	//找到空闲对象的地址
	uintptr v = freeIndex* s->elemsize + s->startaddr;
	s->allocCount++;
	if( (uintptr)s->allocCount > s->nelems ){
		throw("s.allocCount > s.nelems")
	}
	return v;
}
/**
 * 本地缓存不够用了，需要从中心缓存获取span
 * @param c
 * @param spc
 */
 void cache_refill(cache* c,spanclass spc)
 {
// 	printf("cache not enough!\n");
    heap* h = &heap_;
	// Return the current cached span to the central lists.
	span* s = c->alloc[spc];

	if( (uintptr)s->allocCount != s->nelems ){
		throw("refill of span with free space remaining\n" );
	}
	if( s != &emptyspan ){
		// Mark this span as no longer cached.
		if( s->sweepgen != heap_.sweepgen+3 ){
			throw("bad sweepgen in refill\n")
		}
		atomic_Store(&s->sweepgen,heap_.sweepgen);
	}

	// Get a new cached span from the central lists.
	// 本地不够用了，需要从中央链表上进行分配
	s = central_cacheSpan(&heap_.centrals[spc]);
	if( s == NULL ){
		throw("out of memory")
	}

	if( (uintptr)s->allocCount == s->nelems ){
		throw("span has no free space")
	}

	// Indicate that this span is cached and prevent asynchronous
	// sweeping in the next sweep phase. 表明这个span被缓存，防止在下一个扫描阶段进行异步扫描
	s->sweepgen = heap_.sweepgen +  3;

	c->alloc[spc] =  s;
}
/**
 * 对应: func allocmcache() *mcache
 * @return
 */
cache* allocmcache()
{
	lock(&heap_.locks);
	cache* c = fixalloc_alloc(&heap_.cachealloc);
	c->flushGen = heap_.sweepgen;
	unlock(&heap_.locks);
	//初始化空的span 方便判断
    for(int i = 0 ; i < numSpanClasses ; i++){
        c->alloc[i] = &emptyspan;
	}
	return c;
}

/**
 * 在gc的时候需要刷新所有p本地的m，清空
 * @param c
 */
void cache_releaseAll(cache* c)
{

	for(int i = 0 ; i < numSpanClasses ; i++){
		span* s = c->alloc[i];
		if ( s != &emptyspan) {
			central_uncacheSpan(&heap_.centrals[i],s);
			c->alloc[i] = &emptyspan;
		}
	}
	//这是一个刷新操作，刷新本地缓存后才可以进行清除工作
	atomic_Store(&c->flushGen,heap_.sweepgen);

	// Clear tinyalloc pool.
	c->tiny = 0;
	c->tinyoffset = 0;
}