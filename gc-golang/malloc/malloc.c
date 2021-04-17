#include "../heap/heap.h"
#include "../mgc/bitmap.h"
#include "malloc.h"
#include "../sys/gpm.h"

uintptr physPageSize;

//初始化 arean heap  span 等
void mallocinit()
{
	//初始化堆
	heap_init();

	//给当前mcache 设置缓存
	_g_ = malloc(sizeof(g));
    _g_->m = malloc(sizeof(m));
    _g_->m->mallocing  = 0;
    _g_->m->mcache = allocmcache();

    //初始成员变量
    array_init(&heap_.allspans,ARRAY_SIZE, sizeof(span*));
    array_init(&heap_.allarenas,ARRAY_SIZE, sizeof(uint8));
    mutex_init(&heap_.locks);

    //初始化预留的内存
    for ( int i = 0x7f; i >= 0; i-- ){
		uintptr p ;
		//只支持linux
		p = (uintptr )i<<40 | uintptrMask&(0x00c0<<32);
		arenaHint* hint = fixalloc_alloc(&heap_.arenaHintAlloc);
		hint->addr = p;
		//串联成链表
		hint->next = heap_.arenaHints;
		heap_.arenaHints = hint;
	}

}

/**
 *
 * @param size
 * @param needzero
 * @param noscan
 * @return
 */
span* largeAlloc(uintptr size, uint8 needzero, uint8 noscan)
{

	//检测是否内存溢出
	if( size + pageSize < size ){
		throw("out of memory\n");
	}
	uintptr npages;
	span*	s;

	npages = size >> pageShift; //计算页索引
	if( size & PageMask != 0 ) {
		npages ++;
	}
	//开始分配大内存了
	s = heap_alloc(npages, makeSpanClass(0,noscan), true, needzero);
	if( s == NULL ){
		//如果直接从全局堆上无法获取到内存块的话 就溢出了
		throw("out of memory\n");
	}
	s->limit = s->startaddr + size;
	heapBits h = hb_heapBitsForAddr(s->startaddr);
	hb_initSpan(h,s);
	return s;
}
