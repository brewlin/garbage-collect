/**
 *@ClassName gc_sweepbuf
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/12 0012 下午 4:36
 *@Version 1.0
 **/
#include "gc_sweepbuf.h"
#include "../span/span.h"
#include "../atomic/atomic.h"
#include "../sys/sysapi.h"

#define gcSweepBlockEntries    512 // 4KB on 64-bit
#define gcSweepBufInitSpineCap 256 // Enough for 1GB heap on 64-bit
#define CacheLinePadSize	   64

typedef struct {
    span* spans[gcSweepBlockEntries];
}gcSweepBlock;

/**
 *
 * @param b
 * @param s
 */
void gcsweepbuf_push(gcSweepBuf* b,span* s)
{

	// Obtain our slot.
	uint32 cursor = (uintptr)(atomic_Xadd(&b->index, +1) - 1);
	uint32 top    = cursor / gcSweepBlockEntries;
	uint32 bottom = cursor%gcSweepBlockEntries;

	// Do we need to add a block?
	uintptr spineLen = atomic_Loaduintptr(&b->spineLen);
	gcSweepBlock* block;

retry:
	if ( top < spineLen )
	{
		void* spine  = atomic_Loadp(&b->spine);
		void* blockp = spine + ptrSize*top;
		block = (gcSweepBlock*)(atomic_Loadp(blockp));
	} else {
		lock(&b->spineLock);
		spineLen = atomic_Loaduintptr(&b->spineLen);
		if (top < spineLen ){
			unlock(&b->spineLock);
			goto retry;
		}
		//满了
		if (spineLen == b->spineCap ){
			// Grow the spine.
			uintptr newCap = b->spineCap * 2;
			if (newCap == 0 ){
				newCap = gcSweepBufInitSpineCap;
			}
			//申请一块堆外的持久化内存
			void* newSpine = sys_fixalloc(newCap*ptrSize,64);
			if (b->spineCap != 0 ){
				// Blocks are allocated off-heap, so
				// no write barriers.
				memmove(newSpine, b->spine, b->spineCap*ptrSize);
			}
			// Spine is allocated off-heap, so no write barrier.
			atomic_StorepNoWB(&b->spine, newSpine);
			b->spineCap = newCap;
			// FIXME: 这里说的是old内存没有多大，即使泄漏了也没有影响
		}

		block = sys_fixalloc(sizeof(gcSweepBlock),CacheLinePadSize);
		void* 		 blockp = b->spine + ptrSize*top;
		// Blocks are allocated off-heap, so no write barrier.
		atomic_StorepNoWB(blockp, block);
		atomic_Storeuintptr(&b->spineLen, spineLen+1);
		unlock(&b->spineLock);
	}
	// We have a block. Insert the span.
	block->spans[bottom] = s;
}

// pop removes and returns a span from buffer b, or NULL if b is empty.
// pop is safe to call concurrently with other pop operations, but NOT
// to call concurrently with push.
span* gcsweepbuf_pop(gcSweepBuf* b)
{
	uint32 cursor = atomic_Xadd(&b->index, -1);
	if ((int32)cursor < 0 ){
		atomic_Xadd(&b->index, +1);
		return NULL;
	}
	// There are no concurrent spine or block modifications during
	// pop, so we can omit the atomics.
	uint32 top = cursor / gcSweepBlockEntries;
	uint32 bottom = cursor%gcSweepBlockEntries;
	gcSweepBlock** blockp = b->spine + ptrSize*top;
	gcSweepBlock*  block  = *blockp;
	span* s = block->spans[bottom];
	// Clear the pointer for block(i).
	block->spans[bottom] = NULL;
	return s;
}
