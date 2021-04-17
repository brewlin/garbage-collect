#include "bitmap.h"
#include "../sys/gc.h"
#include "bitarena.h"
#include "../sys/gpm.h"
#include "../heap/heap.h"
#include "mark.h"
#include "../atomic/atomic.h"
#include "../heap/arena.h"
#include "gc_work.h"

bool useCheckmark = false;
/**
 * 如果obj还没有被标记过，则直接进行标记为灰色 丢入队列里 等待转为黑色
 * @param obj
 * @param s
 * @param gcw
 * @param objIndex
 */
void greyobject(uintptr obj, span *s, gcWork* gcw, uintptr objIndex)
{
    //obj 必须是对象的首地址，所以这里要检查下是否对齐
    if ( obj & (ptrSize-1) != 0) {
        throw("greyobject: obj not pointer-aligned")
    }
    //找到对象对应的标记位图
    markBits mbits = span_markBitsForIndex(s,objIndex);
    //再次检查一下该对象状态是否正常
    if(span_isFree(s,objIndex)){
        throw("marking free object")
    }
    //看看是否已经标记过了
    if(*mbits.bytep & mbits.mask != 0){
        return;
    }
    //开始标记
    atomic_Or8(mbits.bytep, mbits.mask);

    // Mark span.
    uintptr pageIdx;
    uint8   pageMask;
    heapArena* arena = pageIndexOf(s->startaddr,&pageIdx,&pageMask);
    // FIXME: 标记span ? 没搞懂意思
    if (arena->pageMarks[pageIdx] & pageMask == 0 ){
        atomic_Or8(&arena->pageMarks[pageIdx], pageMask);
    }
    //如果当前对象 为不需要扫描的类型，则直接转换为黑色，完成标记过程
    if( (s->spanclass&1) != 0){
        gcw->bytesMarked += (uint64)s->elemsize;
        return;
    }
    //将当前需要扫描的对象加入队列，等待跟进一步的扫描
    if(!gcwork_putFast(gcw,obj)){
        gcwork_put(gcw,obj);
    }
}
/**
 * 找到地址p对于与堆里对象的起始地址，然后再是其对应的span,span中的索引
 * @param p
 * @param ss
 * @param objIndex
 * @return uintptr
 */
uintptr findObject(uintptr p, span** ss,uintptr* objIndex)
{
    span* s = heap_spanOf(p);
    *ss     = s;
    uintptr base = 0;
    // If p is a bad pointer, it may not be in s's bounds.
    if (s == NULL || p < s->startaddr || p >= s->limit || s->state != mSpanInUse ){
        return NULL;
    }
    // If this span holds object of a power of 2 size, just mask off the bits to
    // the interior of the object. Otherwise use the size to get the base.
    if ( s->basemask != 0 ){
        // optimize for power of 2 sized objects.
        base = s->startaddr;
        base = base + (p-base)&((uintptr)s->basemask);
        *objIndex = (base - s->startaddr) >> s->divshift;
        // base = p & s.baseMask is faster for small spans,
        // but doesn't work for large spans.
        // Overall, it's faster to use the more general computation above.
    } else {
        base = s->startaddr;
        //如果计算出来的p所属的对象首地址异常，则根据objid 从新计算首地址（难道不是应该丢弃么？）
        if ( p-base >= s->elemsize ){
            // n := (p - base) / s.elemsize, using division by multiplication
            *objIndex = (uintptr)(p-base) >> s->divshift * (uintptr)s->divmul >> s->divshift2;
            base += *objIndex * s->elemsize;
        }
    }
    return base;
}
/**
 * 通常用于扫描 非 heap根
 * @param uintptr
 * @param uint8
 * @param gcWork
 * @param stackScanState
 * @return
 */
//void scanblock(uintptr b0,uintptr n0,uint8* ptrmask, gcWork* gcw,stackScanState* stk)
//{
//
//    // Use local copies of original parameters, so that a stack trace
//    // due to one of the throws below shows the original block
//    // base and extent.
//    uintptr b = b0;
//    uintptr n = n0;
//
//    for ( uintptr i = 0; i < n; ){
//        // Find bits for the next word.
//        uint32 bits = *(ptrmask+(i/(ptrSize*8)));
//        if ( bits == 0 ){
//            i += ptrSize * 8;
//            continue
//        }
//        for ( int j = 0; j < 8 && i < n; j++ ){
//            if ( bits&1 != 0 ){
//                // Same work as in scanobject; see comments there.
//                uintptr p = *(uintptr*)(b + i);
//                if ( p != 0 ) {
//                    span* s;
//                    uintptr objIndex;
//                    uintptr obj = findObject(p,&s,&objIndex);
//
//                    //如果不属于heap对象，则可能是栈对象
////                    if(obj != 0){
//                        greyobject(obj,span, gcw, objIndex);
////                    } else if stk != NULL && p >= stk.stack.lo && p < stk.stack.hi {
////                        stk.putPtr(p)
////                    }
//                }
//            }
//            bits >>= 1;
//            i += ptrSize;
//        }
//    }
//}
