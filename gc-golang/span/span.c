/**
 *@ClassName span
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/30 0030 下午 5:41
 *@Version 1->0
 **/
#include "span.h"
#include "../sys/gc.h"
#include "../malloc/malloc.h"
#include "../sys/sysapi.h"

/**
 * 检查index对象是否已经被释放了，在标记对象的时候会调用当前方法来检查obj状态是否合法
 * @param s
 * @param index
 * @return true（已经释放了） false(正常已分配的对象)
 */
bool    span_isFree(span* s ,uintptr  index)
{
    //index 小于 freeindex那么一定合法,index对象一定是属于分配的状态
    if(index < s->freeindex){
        return false;
    }
    //大于的话需要判断一下, 需要根据位图来判断是否分配了
    uint8*  b     = s->allocBits;
    uint8*  bytep = b + index / 8;
    uint8   mask  = 1 << (index % 8);

    //判断对应的位是否置一
    return (*bytep & mask ) == 0;
}

/**
 *
 * @param s
 * @param size
 * @param n
 * @param total
 */
void span_layout(span* s ,uintptr* size,uintptr* n,uintptr* total)
{
	*total = s->npages << _PageShift;
	*size  = s->elemsize;
	if ( *size > 0 ){
		*n = *total / *size;
	}
	return;
}
/**
 * 
 * @param base 
 * @param npages 
 */
void span_init(span* s,uintptr base,uintptr npages)
{
    // span is *not* zeroed->
    s->next = NULL;
    s->prev = NULL;
    s->list = NULL;
    s->startaddr = base;
    s->npages = npages;
    s->allocCount = 0;
    s->spanclass = 0;
    s->elemsize = 0;
    s->state = mSpanDead;
    s->scavenged = false;
    s->needzero = 0;
    s->freeindex = 0;
    s->allocBits = NULL;
    s->gcmarkBits = NULL;
}
// physPageBounds returns the start and end of the span
// rounded in to the physical page size.
/**
 * 计算span内存范围，在物理页上面进行四舍五入对齐
 * @param start
 * @param end
 */
void span_ppbounds(span* s ,uintptr* start,uintptr* end)
{
    *start = s->startaddr;
    *end   = *start + s->npages << pageShift;
    if ( physPageSize > pageSize ) {
        *start = (*start + physPageSize - 1) &~ (physPageSize - 1);
        *end   = *end &~ (physPageSize - 1);
    }
}
/**
 * 计算span已经归还给 os多少字节数内存
 * @param s
 * @return
 */
uintptr span_released(span* s)
{
    if(!s->scavenged) return 0 ;
    uintptr start,end;
    span_ppbounds(s,&start,&end);

    return end - start;
}
/**
 *
 * @param s
 * @return
 */
uintptr span_scavenge(span* s)
{
    // start and end must be rounded in, otherwise madvise
    // will round them *out* and release more memory
    // than we want.
    uintptr start,end,released;
    span_ppbounds(s,&start,&end);
    if( end <= start ){
    // start and end don't span a whole physical page.
        return 0;
    }
    released = end - start;
    s->scavenged = true;
    sys_unused(start,released);
    return released;
}
/**
 * 当前位图窗口不够用了，需要移动下一个窗口
 */
 void span_refillAllocCache(span* s,uintptr whichByte)
{
 	//8字节 窗口
 	byte* bytes = s->allocBits + whichByte;
	uint64 aCache = 0;
	aCache |= (uint64)bytes[0];
	aCache |= (uint64)bytes[1] << (1 * 8);
	aCache |= (uint64)bytes[2] << (2 * 8);
	aCache |= (uint64)bytes[3] << (3 * 8);
	aCache |= (uint64)bytes[4] << (4 * 8);
	aCache |= (uint64)bytes[5] << (5 * 8);
	aCache |= (uint64)bytes[6] << (6 * 8);
	aCache |= (uint64)bytes[7] << (7 * 8);
	//计算allocBits的补码 给  allocCache
	s->allocCache = ~aCache;
}

/**
 * 根据位图来计算当前span 空闲内存的索引
 * @param s
 * @return
 */
uintptr span_nextFreeIndex(span* s)
{
	uintptr sfreeindex = s->freeindex;
	uintptr snelems    = s->nelems;
	if( sfreeindex == snelems ) {
		return sfreeindex;
	}
	if( sfreeindex > snelems ){
		throw("s->freeindex > s->nelems")
	}

	uint64 aCache = s->allocCache;
	int bitIndex  = ctz64(aCache);
	while( bitIndex == 64 ){
		// Move index to start of next cached bits.
		sfreeindex = (sfreeindex + 64) &~ (64 - 1);
		if( sfreeindex >= snelems ){
			s->freeindex = snelems;
			return snelems;
		}
		uintptr whichByte = sfreeindex / 8;
		// Refill s.allocCache with the next 64 alloc bits->
		span_refillAllocCache(s,whichByte);
		aCache = s->allocCache;
		bitIndex = ctz64(aCache);
		// nothing available in cached bits
		// grab the next 8 bytes and try again.
	}
	uintptr result = sfreeindex + (uintptr)(bitIndex);
	if( result >= snelems ){
		s->freeindex = snelems;
		return snelems;
	}

	s->allocCache >>= (uint)(bitIndex + 1);
	sfreeindex = result + 1;

	if (sfreeindex%64 == 0 && sfreeindex != snelems ){
		// We just incremented s.freeindex so it isn't 0.
		// As each 1 in s.allocCache was encountered and used for allocation
		// it was shifted away. At this point s.allocCache contains all 0s.
		// Refill s.allocCache so that it corresponds
		// to the bits at s.allocBits starting at s.freeindex.
		uintptr whichByte = sfreeindex / 8;
		span_refillAllocCache(s,whichByte);
	}
	s->freeindex = sfreeindex;
	return result;
}

/**
 *
 * @param list
 * @param span1
 */
void spanlist_remove(spanlist* list,span* s)
{
	if ( s->list != list ) {
		throw("mSpanList.remove")
	}
	if ( list->first == s ) {
		list->first = s->next;
	} else {
		s->prev->next = s->next;
	}
	if ( list->last == s ){
		list->last = s->prev;
	} else {
		s->next->prev = s->prev;
	}
	s->next = NULL;
	s->prev = NULL;
	s->list	= NULL;
}

/**
 * 
 * @param list 
 * @param s 
 */
void spanlist_insertback(spanlist* list,span* s)
{
	if ( s->next != NULL || s->prev != NULL || s->list != NULL ){
		throw("mSpanList.insertBack")
	}
	s->prev = list->last;
	if ( list->last != NULL ){
		// The list contains at least one span.
		list->last->next = s;
	} else {
		// The list contains no spans, so this is also the first span.
		list->first = s;
	}
	list->last = s;
	s->list = list;
}

/**
 * 
 * @param list 
 * @param s 
 */
void spanlist_insert(spanlist* list,span* s)
{
	if(s->startaddr == 0)
		throw("s is unalloced!")
	if (s->next != NULL || s->prev != NULL || s->list != NULL){
		throw("mSpanList.insert")
	}
	s->next = list->first;
	if ( list->first != NULL ){
		// The list contains at least one span; link it in.
		// The last span in the list doesn't change.
		list->first->prev = s;
	} else {
		// The list contains no spans, so this is also the last span.
		list->last = s;
	}
	list->first = s;
	s->list = list;
}

/**
 * 扫描位图 统计已经分配了多少对象
 */
 int span_countAlloc(span* s)
 {
	int count = 0;
	uintptr maxIndex = s->nelems / 8;
	for(uintptr i = 0; i < maxIndex; i++ ){
	    //依次访问每个字节
		uint8 mrkBits = *(s->gcmarkBits + i);
		count += (int)(oneBitCount[mrkBits]);
	}
	uintptr bitsInLastByte = s->nelems % 8;
	if ( bitsInLastByte != 0 )
	{
		uint8 mrkBits = *(s->gcmarkBits + maxIndex);
		uint8 mask 	  = (uint8)((1 << bitsInLastByte) - 1	);
		uint8 bits 	  = mrkBits & mask;
		count += (int)(oneBitCount[bits]);
	}
	return count;
}
