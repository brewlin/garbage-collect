/**
 *@ClassName heap
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 3:43
 *@Version 1.0
 **/

#include "heap.h"

#include "../cache/cache.h"
#include "../malloc/malloc.h"
#include "arena.h"
#include "../sys/sysapi.h"
#include "../atomic/atomic.h"
#include "../sys/gpm.h"
#include "../mgc/gc_sweepbuf.h"

heap heap_;

//第一次span申请的时候加入 heap.allspans 中管理
void recordspan(void* vh, void* p) {
    heap* h = (heap*)(vh);
    span* s = (span*)(p);

    //将新申请的span 插入全局heap 管理
    void** insert = array_push(&h->allspans);
    *insert = s;
}

/**
 * 堆初始化, 对应golang: func (h *mheap) init()
 * @param h
 */
void heap_init()
{
    heap* h = &heap_;
    //初始化各种分配器
    fixalloc_init(&h->treapalloc, sizeof(treapNode),NULL,NULL); // 对应golang: func (f *fixalloc) init
    fixalloc_init(&h->spanalloc, sizeof(span),recordspan,h);
    fixalloc_init(&h->cachealloc, sizeof(cache),NULL,NULL);

    fixalloc_init(&h->specialfinalizeralloc, 0,NULL,NULL);
    fixalloc_init(&h->specialprofilealloc, 0,NULL,NULL);
    fixalloc_init(&h->arenaHintAlloc, sizeof(arenaHint),NULL,NULL);

    //mcentral 中央空闲链表初始化
    for(int i = 0 ; i < numSpanClasses ;i ++){
        central_init(&h->centrals[i],i);
    }
}

/**
 *
 * @param npage
 * @param spanclass
 * @param large
 * @param needzero
 * @return
 */
span* heap_alloc(uintptr npage, uint8 spanclass, bool large , bool needzero)
{
    span* s = heap_alloc_m(npage,spanclass,large);
    //内存清0
    if( s != NULL) {
        //是否需要内存清零
        if( needzero && s->needzero != 0 )
            memset(s->startaddr,0,s->npages << pageShift);
        s->needzero = 0;
    }
    return s;
}
/**
 * 1. 本地mcache mspan不够用了 找 mcentral
 * 2. mcentral也不够用了 调用当前heap分配器分配一个足量的mspan
 * @param npage
 * @param sc
 * @param large
 * @return
 */
span* heap_alloc_m(uintptr npage,spanclass spanc,bool large)
{

    //TODO:为了防止堆过量的增长，需要每次分配前都会试着回收一部分内存
    //if h.Sweepdone == 0 {
    //	h.Reclaim(npage)
    //}
    heap* h = &heap_;

    //加锁
    lock(&h->locks);
    // transfer stats from cache to global
    //memstats.heap_scan += uint64(_g_.m.Mcache.local_scan)
    //_g_.m.Mcache.local_scan = 0
    //memstats.tinyallocs += uint64(_g_.m.Mcache.local_tinyallocs)
    //_g_.m.Mcache.local_tinyallocs = 0
    //尝试去分配npage个页
    span* s = heap_allocSpanLocked(npage);
	if ( s != NULL )
	{
        atomic_Store(&s->sweepgen,h->sweepgen);
        //将新分配的s加入监控，在gc清扫的时候会检查
        gcsweepbuf_push(&h->sweepSpans[h->sweepgen/2%2],s);
		s->state = mSpanInUse;
		s->allocCount = 0;
		s->spanclass  = spanc;
		int8 sc = sizeclass(spanc);
		if ( sc == 0 ) {
			s->elemsize = s->npages << pageShift;
			s->divshift = 0;
			s->divmul   = 0;
			s->divshift2 = 0;
			s->basemask = 0;
		} else {
			s->elemsize = (uintptr)(class_to_size[sc]);
			divMagic* m = &class_to_divmagic[sc];
			s->divshift = m->shift;
			s->divmul   = m->mul;
			s->divshift2 = m->shift2;
			s->basemask = m->baseMask;
		}

        // Mark in-use span in arena page bitmap.
		uintptr pageIdx;
		uint8   pageMask;
		heapArena* arena = pageIndexOf(s->startaddr,&pageIdx,&pageMask);
		arena->pageInuse[pageIdx] |= pageMask;

		// update stats, sweep lists
		h->pagesInuse += (uint64)(npage);
		if( large ){
		    h->largealloc  += (uint64)(s->elemsize);
		    h->nlargealloc++;
		}
	}
	// heap_scan and heap_live were updated.
	// TODO: gc mark phase
    //	if gcBlackenEnabled != 0 {
    //		gcController.revise()
    //	}

    unlock(&h->locks);
	return s;
}
/**
 * heap 分配给定页数的 mspan
 * @param npage
 * @return
 */
span* heap_allocSpanLocked(uintptr npage)
{
    //从堆中获取span
    heap* h = &heap_;
    span* s = heap_pickFreeSpan(npage),*t;
    if( s != NULL ){
//        printf("heap got one %d\n",s->npages);
        goto haveSpan;
    }

    //获取Mspan 失败了，可能堆不够用了，需要扩充一下
    if(!heap_grow(npage)){
        //如果扩充heap失败，则一直返回权利到最上层，也就是从mcache那个地方会抛出 out of memory
        return NULL;
    }
    s = heap_pickFreeSpan(npage);
    if ( s != NULL ) {
        goto haveSpan;
    }
    //宿主机 内存不够了直接中断当前程序
    throw("grew heap, but no adequate free span found");

haveSpan:
    //更新当前span的使用状态
    if (s->state != mSpanFree ) {
        throw("candidate Mspan for allocation is not free")
    }
    //分配的页数比实际需要的小 ，报错
    if (s->npages < npage ) {
        throw("candidate Mspan for allocation is too small")
    }
    //分配的页数比实际需要的大，减掉一部分回收
    if (s->npages > npage ) {
        t = (span*)fixalloc_alloc(&h->spanalloc);//从heap之外申请一个span内存用于 存储s多出的那部分内存

        //太浪费了，裁掉多余的page 放回heap中去
        // t->npages = s->npages - npage
        // s->npages = npage;
        span_init(t,s->startaddr+(npage<<pageShift),s->npages-npage);
        s->npages = npage;

        //更新之前的映射关系
        heap_setSpan(t->startaddr-1,s);
        heap_setSpan(t->startaddr,t);
        heap_setSpan(t->startaddr+t->npages*pageSize-1,t);

        t->needzero = s->needzero;

        // If s was scavenged, then t may be scavenged->
        //计算span的内存边界（根据物理页对齐）
        uintptr start,end;
        span_ppbounds(t,&start,&end);

        if (s->scavenged && start < end ) {
            t->scavenged = true;
        }
        s->state = mSpanManual; // prevent coalescing with s
        t->state = mSpanManual;
        //需要释放一部分页到内存去
        heap_freeSpanLocked(t,false,false,s->unusedsince);
        s->state = mSpanFree;
    }
    // "Unscavenge" s only AFTER splitting so that
    // we only SysUsed whatever we actually need->
    if (s->scavenged ) {
        // SysUsed all the pages that are actually available
        // in the span-> Note that we don't need to decrement
        // heap_released since we already did so earlier->
        //建议该mmap内存 可以价值加载到内存了
        sys_used(s->startaddr, s->npages<<pageShift);
        s->scavenged = false;
    }
    s->unusedsince = 0;

    heap_setSpan(s->startaddr,s);

    //println("spanalloc", hex(s->start<<PageShift))
    if (s->list != NULL)
        throw("still in list")
    return s;
}

/**
 * 从堆中分配一个span
 * @param npage
 * @return
 */
span* heap_pickFreeSpan(uintptr npage){

    //从heap的两个二叉树中寻找大于申请页的span
    heap* h = &heap_;
    treapNode* tf = treap_find(&h->free,npage);
    treapNode* ts = treap_find(&h->scav,npage);

    //通过在两颗树上 分别搜索可用的span
    //best fit:最终选择相对更合适的那个span
    span* s = NULL;
    if ( tf != NULL && (ts == NULL || tf->spankey->npages <= ts->spankey->npages)) {
        s = tf->spankey;
        treap_removeNode(&h->free,tf);
    } else if ( ts != NULL && (tf == NULL || tf->spankey->npages > ts->spankey->npages) ) {
        s = ts->spankey;
        treap_removeNode(&h->scav,ts);
    }
    return s;
}
/**
 * 给堆中至少添加npage页内存
 * 因为堆操作是全局的，所以多线程下是需要加锁的,锁住h
 * @param npage
 * @return
 */
bool heap_grow(uintptr  npage)
{
    heap* h = &heap_;
    //扩充堆
    uintptr ask = npage << pageShift; //将页数转换为字节大小  page *= 4k(4096| << 13) 每页大小为4k
    //1-> 首先尝试从更大的 arena 中获取满足的内存
    //2-> 其次在向操作系统获取内存
    uintptr* v = NULL,size;
    v = heap_sysAlloc(ask,&size);
    if ( v == NULL) {
        printf("runtime: out of memory: cannot allocate %ld -byte block (%ld in use)\n",ask,0);
        return false;
    }

    //从空闲treap上面清除一些页面来弥补我们刚刚从虚拟内存空间中分配了一些空间，
    //倾向于首先清除 一些大spans，因为清除成本与sysunused调用数量成正比，而不是释放页面
    heap_scavengeLargest(size);

    // Create a fake "in use" span and free it, so that the
    // right coalescing happens->
    span* s = fixalloc_alloc(&h->spanalloc);
    span_init(s,v,size/pageSize);

    heap_setSpans(s->startaddr,s->npages,s);
    s->sweepgen = h->sweepgen;
    s->state = mSpanInUse;
    h->pagesInuse += (uint64)(s->npages);
    //把刚从操作系统上申请的内存放到全局heap上，
    //业务层再重新尝试分配
    heap_freeSpanLocked(s,false,true,0);
    return true;
}


/**
 * s must be on the busy list or unlinked.
 * @param s
 * @param acctinuse
 * @param acctidle
 * @param unusedsince
 */
void heap_freeSpanLocked(span* s,bool acctinuse,bool acctidle,int64 unusedsince)
{
    heap* h = &heap_;
	switch (s->state )
	{
	case mSpanManual:
		if ( s->allocCount != 0 ) {
			throw("mheap->freeSpanLocked - invalid stack free");
		}
        break;
	case mSpanInUse:
		if ( s->allocCount != 0 || s->sweepgen != h->sweepgen ) {
//			pr(int)("mheap->freeSpanLocked - span %p", s, " ptr ", hex(s->base()), " allocCount ", s->allocCount, " sweepgen ", s->sweepgen, "/", h->sweepgen, "\n");
			throw("mheap->freeSpanLocked - invalid free")
		}
		h->pagesInuse -= (uint64)(s->npages);

		// Clear in-use bit in arena page bitmap->
		uintptr    pageIdx;
		uint8      pageMask;

		heapArena* arena = pageIndexOf(s->startaddr,&pageIdx,&pageMask);
		//更新页标记位 标记已经被回收并且可用了
		arena->pageInuse[pageIdx] = arena->pageInuse[pageIdx] &~ pageMask;
        break;
	default:
		throw("mheap->freeSpanLocked - invalid span state")
	}

	s->state = mSpanFree;

	// Stamp newly unused spans-> The scavenger will use that
	// info to potentially give back some pages to the OS->
	s->unusedsince = unusedsince;
	if ( unusedsince == 0 ) {
	    //TODO: NANOTIME() implemention
//		s->unusedsince = nanotime();
	}

	// Coalesce span with neighbors
	// 释放时看能不能当前span 和相邻的span结合起来组成更大的span
	heap_coalesce(s);

	//将span放入空闲二叉树上 等待重新利用
	if ( s->scavenged ) {
	    treap_insert(&h->scav,s);
	} else {
	    treap_insert(&h->free,s);
	}
}
/**
 * 将 span 归还到堆里
 * @param s
 * @param large
 */
void heap_freeSpan(span* s , bool large)
{
    heap* h = &heap_;
    m* mp = getg()->m;
    lock(&h->locks);
    mp->mcache->local_scan = 0;

    mp->mcache->local_tinyallocs = 0;
    if ( gcBlackenEnabled != 0 ){
        // heap_scan changed.
//        gcController.revise()
    }
    heap_freeSpanLocked(s,true,true,0);
    unlock(&h->locks);
}

/**
 *
 * @param npage
 * @return
 */
span* heap_allocManual(uintptr npage)
{
    heap* h = &heap_;
	lock(&h->locks);
	span* s = heap_allocSpanLocked(npage);
	if ( s != NULL) {
		s->state = mSpanManual;
		s->allocCount = 0;
		s->spanclass = 0;
		s->nelems = 0;
		s->elemsize = 0;
		s->limit = s->startaddr + s->npages << _PageShift;
	}

	// This unlock acts as a release barrier. See mheap.alloc_m.
	unlock(&h->locks);
	return s;
}