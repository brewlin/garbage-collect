#include "gc_work.h"
#include "../atomic/atomic.h"
#include "../heap/heap.h"

// get to really high addresses and panic if it does.
#define addrBits 48
// In addition to the 16 bits taken from the top, we can take 3 from the
// bottom, because node must be pointer-aligned, giving a total of 19 bits
// of count.
#define cntBits  (64 - addrBits + 3)
#define workbufAlloc (32 << 10)


//全局work 管理器
_work work;

uint64 lfstackPack(lfnode* node, uintptr cnt)
{
	return ((uint64)node)<<(64-addrBits) | (uint64)(cnt&(1<<cntBits-1));
}
lfnode* lfstackUnpack(uint64 val)
{
    return (lfnode*)((int64)val >> cntBits << 3);
}

// lfnodeValidate panics if node is not a valid address for use with
// lfstack.push. This only needs to be called when node is allocated.
void lf_lfnodeValidate(lfnode* node)
{
    if ( lfstackUnpack(lfstackPack(node, ~(uintptr)0)) != node ){
        throw("bad lfnode address\n");
    }
}

/**
 *
 * @param head
 * @param node
 */
void lf_push(uint64 *head,lfnode* node)
{
	node->pushcnt++;
	uint64 new = lfstackPack(node, node->pushcnt);
	lfnode* node1 = lfstackUnpack(new);
	if ( node1 != node ){
		throw("lfstack.push\n");
	}
	while(true){
	    uint64 old = *head;
	    node->next = old;
		if ( atomic_Cas64((uint64*)(head), old, new)) {
			break;
		}
	}
}
/**
 * @param head
 * @return
 */
void* lf_pop(uint64* head)
{
	while(true)
	{
		uint64 old = *head;
		if( old == 0 ){
			return NULL;
		}
		lfnode* node = (lfnode*)((int64)old >> cntBits << 3);
		uint64  next = node->next;
		if ( atomic_Cas64(head, old, next) ){
		    return node;
		}
	}
}
/**
 * @return
 */
workbuf* getempty()
{
	workbuf* b = NULL;
	if ( work.empty != 0 ){
	    //先从空闲链表上进行分配
	    workbuf* b = lf_pop(&work.empty);
		if ( b != NULL && b->nobj != 0 ) {
		    throw("work buf is not empty!\n")
		}
	}
	if ( b == NULL ){
		// Allocate more workbufs.
		span* s = NULL;
		if ( work.wbufSpans.free.first != NULL ) {
			lock(&work.wbufSpans.locks);
			s = work.wbufSpans.free.first;
			if (s != NULL ){
			    spanlist_remove(&work.wbufSpans.free,s);
			    spanlist_insert(&work.wbufSpans.busy,s);
			}
			unlock(&work.wbufSpans.locks);
		}
		if ( s == NULL ){
		    s = heap_allocManual(workbufAlloc/pageSize);
			if (s == NULL) {
				throw("out of memory\n");
			}
			// Record the new span in the busy list.
			lock(&work.wbufSpans.locks);
			spanlist_insert(&work.wbufSpans.busy,s);
			unlock(&work.wbufSpans.locks);
		}
		// 将之前申请的span拆分成功 workbufs结构，返回一个给调用方，其他的加入链表上等待使用
		for (uintptr i = 0; (i + _WorkbufSize) <= workbufAlloc; i += _WorkbufSize ){
			workbuf* newb = (workbuf*)(s->startaddr + i);
			newb->nobj    = 0;
			lf_lfnodeValidate(&newb->node);

			//第一个返回给调用方使用
			if (i == 0 ){
				b = newb;
			} else {
			    if(b->nobj != 0){
			        throw("work buf is not empty yet\n");
			    }
			    //加入线性链表管理
			    lf_push(&work.empty,newb);
			}
		}
	}
	return b;
}
workbuf* trygetfull()
{
    workbuf* b = lf_pop(&work.full);
	if ( b != NULL ){
	    if(b->nobj == 0){
	        throw("work buf is empty!");
	    }
		return b;
	}
	return b;
}

/**
 * 工作缓存区不够用了需要初始化一个新的缓冲区，来自heap
 * @param w
 */
void gcwork_init(gcWork* w)
{
    w->wbuf1    = getempty();
    w->wbuf2  = trygetfull();

    if (w->wbuf2 == NULL)
    {
        w->wbuf2 = getempty();
    }
}

// putFast does a put and reports whether it can be done quickly
// otherwise it returns false and the caller needs to call put.
/**
 * @param w
 * @param obj
 * @return
 */
bool gcwork_putFast(gcWork* w,uintptr obj)
{
    workbuf* wbuf = w->wbuf1;
	if ( wbuf == NULL) {
		return false;
	} else if ( wbuf->nobj == _workbufObjsize ) {
		return false;
	}

	wbuf->obj[wbuf->nobj] = obj;
	wbuf->nobj++;
	return true;
}
uintptr gcwork_tryGetFast(gcWork* w){
	workbuf* wbuf = w->wbuf1;
	if (wbuf == NULL ){
		return NULL;
	}
	if (wbuf->nobj == 0 ){
		return NULL;
	}

	wbuf->nobj--;
	return wbuf->obj[wbuf->nobj];
}

uintptr gcwork_tryGet(gcWork* w)
{
	workbuf* wbuf = w->wbuf1;
	if (wbuf == NULL ){
	    gcwork_init(w);
		wbuf = w->wbuf1;
		// wbuf is empty at this point.
	}
	if (wbuf->nobj == 0 ){
	    workbuf* tmp = w->wbuf1;
	    w->wbuf1 = w->wbuf2;
	    w->wbuf2 = tmp;
		wbuf = w->wbuf1;
		if (wbuf->nobj == 0 ){
			workbuf* owbuf = wbuf;
			wbuf = trygetfull();
			if (wbuf == NULL ){
				return NULL;
			}
			if(owbuf->nobj != 0) throw("workbuf is not empty!");
			lf_push(&work.empty,&owbuf->node);
			w->wbuf1 = wbuf;
		}
	}

	wbuf->nobj--;
	return wbuf->obj[wbuf->nobj];
}
/**
 * @param w
 * @param obj
 */
void gcwork_put(gcWork* w, uintptr obj)
{
	bool flushed  = false;
	workbuf* wbuf = w->wbuf1;
	if (wbuf == NULL ){
	    gcwork_init(w);
		wbuf = w->wbuf1;
		// wbuf is empty at this point.
	} else if (wbuf->nobj == _workbufObjsize )
	{
	    //work1 满了，需要交换
	    workbuf* t = w->wbuf1;
	    w->wbuf1   = w->wbuf2;
	    w->wbuf2   = t;

		wbuf = w->wbuf1;
		if ( wbuf->nobj == _workbufObjsize){
		    //满了 需要加到全局的 full列表上
		    lf_push(&work.full,&wbuf->node);
			w->flushedWork = true;
			wbuf = getempty();
			w->wbuf1 = wbuf;
			flushed = true;
		}
	}

	wbuf->obj[wbuf->nobj++] = obj;

	//FIXME: 如果灰色对象队列已经满了，可以通知调度器来进行消费队列
	if ( flushed && gcphase == _GCmark ){
//		gcController.enlistWorker()
	}
}