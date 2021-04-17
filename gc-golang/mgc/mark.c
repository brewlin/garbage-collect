#include "../sys/gc.h"
#include "mark.h"
#include "../span/span.h"
#include "../heap/heap.h"
#include "../atomic/atomic.h"

bool useCheckmark = false;

// setMarked sets the marked bit in the markbits, atomically.
void mb_setMarked(markBits* m){
    atomic_Or8(m->bytep,m->mask);
}
/**
 * 标记新分配的内存为黑色(针对在gc阶段中产生的新内存)
 * @param obj
 * @param size
 * @param scanSize
 */
void gc_marknewobject(uintptr obj,uintptr size,uintptr scanSize)
{

	if ( useCheckmark ) { // The world should be stopped so this should not happen.
		throw("gcmarknewobject called while doing checkmark")
	}
	markBits tmb = gc_markBitsForAddr(obj);
	mb_setMarked(&tmb);
//	gcw := &getg().m.p.ptr().gcw
//	gcw.bytesMarked += (uint64)(size)
//	gcw.scanWork += (int64)scanSize)
}
/**
 * @param s
 * @param p
 * @return
 */
uintptr span_objIndex(span* s ,uintptr p)
{
	uintptr byteOffset = p - s->startaddr;
	if (byteOffset == 0 ){
		return 0;
	}
	if (s->basemask != 0 ){
		// s.baseMask is non-0, elemsize is a power of two, so shift by s.divShift
		return byteOffset >> s->divshift;
	}
	return (uintptr)(((uint64)(byteOffset) >> s->divshift) * (uint64)(s->divmul) >> s->divshift2);
}
/**
 *
 * @param p
 * @return
 */
markBits gc_markBitsForAddr(uintptr p)
{
    //找出p对应的span
    span* s = heap_spanOf(p);
    uintptr objIndex = span_objIndex(s,p);
    return span_markBitsForIndex(s,objIndex);
}

/**
 *
 * @param s
 * @param objIndex
 * @return
 */
markBits span_markBitsForIndex(span* s,uintptr objIndex)
{
    //每个位对应一个对象，所以找到该对象所属的 字节
    uint8* bytep = s->gcmarkBits + (objIndex/8);
    //找到上面字节中对应于哪个位
    uint8  mask  = 1 << (objIndex%8);

    markBits mb  = {bytep,mask,objIndex};
    return mb;
}