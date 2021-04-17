/**
 *@ClassName heap_span
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/31 0031 上午 11:44
 *@Version 1.0
 **/
#include "heap.h"
#include "../malloc/malloc.h"
#include "arena.h"
#include "../sys/array.h"

/**
 * merge is a helper which merges other into s, deletes references to other
 * in heap metadata, and then discards it. other must be adjacent to s.
 * @param s
 * @param other
 * @param prescavenged
 */
void coalesce_merge(span*s ,span* other,bool* needsScavenge,uintptr* prescavenged)
{
	heap* h = &heap_;
    // Adjust s via base and npages and also in heap metadata.
    s->npages += other->npages;
    s->needzero |= other->needzero;
    if ( other->startaddr < s->startaddr ) {
        s->startaddr = other->startaddr;
        heap_setSpan(s->startaddr,s);
    } else {
        heap_setSpan(s->startaddr+s->npages*pageSize-1,s);
    }

    // If before or s are scavenged, then we need to scavenge the final coalesced span.
    *needsScavenge = *needsScavenge || other->scavenged || s->scavenged;
    *prescavenged += span_released(other);

    // The size is potentially changing so the treap needs to delete adjacent nodes and
    // insert back as a combined node->
    if ( other->scavenged ) {
        treap_removeSpan(&h->scav,other);
    } else {
        treap_removeSpan(&h->free,other);
    }
    other->state = mSpanDead;
    //回收span
    fixalloc_free(&h->spanalloc,other);
}
/**
 *
 * realign is a helper which shrinks other and grows s such that their
 * boundary is on a physical page boundary.
 * @param a
 * @param b
 * @param other
 */
void coalesce_realign(span* a,span* b,span* other)
{
    heap* h = &heap_;
    // Caller must ensure a.startaddr < b.startaddr and that either a or
    // b is s. a and b must be adjacent. other is whichever of the two is
    // not s.

    // If pageSize <= physPageSize then spans are always aligned
    // to physical page boundaries, so just exit.
    if ( pageSize <= physPageSize ) {
        return;
    }
    // Since we're resizing other, we must remove it from the treap.
    if ( other->scavenged ) {
    	treap_removeSpan(&h->scav,other);	
    } else {
		treap_removeSpan(&h->free,other);	
    }
    // Round boundary to the nearest physical page size, toward the
    // scavenged span.
    uintptr boundary = b->startaddr;
    if ( a->scavenged ) {
        boundary = boundary &~(physPageSize - 1);
    } else {
        boundary = (boundary + physPageSize - 1) &~ (physPageSize - 1);
    }
    a->npages = (boundary - a->startaddr) / pageSize;
    b->npages = (b->startaddr + b->npages*pageSize - boundary) / pageSize;
    b->startaddr = boundary;

    heap_setSpan(boundary-1, a);
    heap_setSpan(boundary, b);

    // Re-insert other now that it has a new size->
    if ( other->scavenged ) {
    	treap_insert(&h->scav,other);
    } else {
		treap_insert(&h->free,other);
    }
}
/**
 * 回收span的时候 检查是否可以进行相邻span合并
 * @param s
 */
void heap_coalesce(span* s)
{
	heap* h = &heap_;

	// We scavenge s at the end after coalescing if s or anything
	// it merged with is marked scavenged.
	bool needsScavenge = false;

	uintptr prescavenged = span_released(s); // number of bytes already scavenged.
	// Coalesce with earlier, later spans.
    span* before = heap_spanOf(s->startaddr - 1);
	if (before != NULL && before->state == mSpanFree)
	{
		if ( s->scavenged == before->scavenged ) {
			coalesce_merge(s,before,&needsScavenge,&prescavenged);
		} else {
		    coalesce_realign(before,s,before);
		}
	}

	// Now check to see if next (greater addresses) span is free and can be coalesced->
    span* after = heap_spanOf(s->startaddr + s->npages*pageSize);
    if ( after != NULL && after->state == mSpanFree ) {
		if ( s->scavenged == after->scavenged ) {
			coalesce_merge(s,after,&needsScavenge,&prescavenged);
		} else {
			coalesce_realign(s,after,after);
		}
	}

	if ( needsScavenge ) {
	    //进行清除、清扫
	    span_scavenge(s);
	}
}

/**
 *
 * @param p
 * @return
 */
span* heap_spanOf(uintptr p)
{
	uint ri = arenaIndex(p);
	if ( arenaL1Bits == 0 ) {
		// If there's no L1, then ri.l1() can't be out of bounds but ri.l2() can.
		if ( arena_l2(ri) >= (uint)(heap_.arenas[0].used) ) {
			return NULL;
		}
	}
//	else {
	    // 目前不支持windows 下多一维数组的情况
//		 If there's an L1, then ri.l1() can be out of bounds but ri.l2() can't.
//		if ( arena_l1(ri) >= (uint)(len(mheap_->arenas)) ) {
//			return NULL;
//		}
//	}
	array* l2 = &heap_.arenas[arena_l1(ri)];
	if ( arenaL1Bits != 0 && l2->addr == NULL ) { // Should never happen if there's no L1.
		printf("span not exist!");
		return NULL;
	}
	heapArena** ha = ARRAY_GET(l2,arena_l2(ri));
	if ( *ha == NULL ) {
		printf("arena and span not exist!");
		return NULL;
	}
	//找到对应的span
	return (*ha)->spans[(p/pageSize)%pagesPerArena];
}
/**
 *
 * @param base
 * @param s
 */
void heap_setSpan(uintptr base,span* s)
{
	uint ai = arenaIndex(base);
	array* arr = &heap_.arenas[arena_l1(ai)];

	heapArena** ha = ARRAY_GET(arr,arena_l2(ai));
	//挂在到 heapArena.spans 中的某一个位置上
	(*ha)->spans[(base/pageSize)%pagesPerArena] = s;
}
// setSpans modifies the span map so [spanOf(base), spanOf(base+npage*pageSize))
// is s.
void heap_setSpans(uintptr base,uintptr npage,span* s )
{
    heap* h = &heap_;
    uintptr  p = base / pageSize;
    uint    ai = arenaIndex(base);
    //找到base地址对应的arena 堆
    array* arr = &h->arenas[arena_l1(ai)];
    heapArena** ha = ARRAY_GET(arr,arena_l2(ai));

    for (uintptr n = 0; n < npage; n++ ) {
        uintptr i = (p + n) % pagesPerArena;
        if ( i == 0 ){
            ai = arenaIndex(base + n*pageSize);
            //找到对应的arena堆区
            arr = &h->arenas[arena_l1(ai)];
            ha  = ARRAY_GET(arr,arena_l2(ai));
        }
        (*ha)->spans[i] = s;
    }
}
// scavengeLargest scavenges nbytes worth of spans in unscav
// starting from the largest span and working down. It then takes those spans
// and places them in scav. h must be locked.
void heap_scavengeLargest(uintptr nbytes)
{
    heap* h = &heap_;
	// Use up scavenge credit if there's any available.
	if ( nbytes > h->scavengeCredit ) {
		nbytes -= h->scavengeCredit;
		h->scavengeCredit = 0;
	} else {
		h->scavengeCredit -= nbytes;
		return;
	}
	// Iterate over the treap backwards (from largest to smallest) scavenging spans
	// until we've reached our quota of nbytes.
	uintptr released = 0;
	treapNode* t = treap_end(&h->free);
	while(t != NULL && released < nbytes)
    {
        span*   s = t->spankey;
        uintptr r = span_scavenge(s);
		if ( r == 0 )
		{
			// Since we're going in order of largest-to-smallest span, this
			// means all other spans are no bigger than s. There's a high
			// chance that the other spans don't even cover a full page,
			// (though they could) but iterating further just for a handful
			// of pages probably isn't worth it, so just stop here.
			//
			// This check also preserves the invariant that spans that have
			// `scavenged` set are only ever in the `scav` treap, and
			// those which have it unset are only in the `free` treap.
			return;
		}
		treapNode* n = treap_pred(t);
		treap_removeNode(&h->free,t);
		// Now that s is scavenged, we must eagerly coalesce it
		// with its neighbors to prevent having two spans with
		// the same scavenged state adjacent to each other.
		heap_coalesce(s);
		t = n;
		treap_insert(&h->scav,s);
		released += r;
	}
	// If we over-scavenged, turn that extra amount into credit.
	if ( released > nbytes ){
		h->scavengeCredit += released - nbytes;
	}
}