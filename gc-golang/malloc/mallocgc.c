#include "malloc.h"
#include "../span/span.h"
#include "../mgc/mark.h"
#include "../sys/gpm.h"
#include "../mgc/gc.h"
#include "../proc/proc.h"

//这里通过allocCache位图 快速获取可用的内存，并更新该位图
//这里不需要更新allocBits 总位图
/**
 * @param s
 * @return
 */
uintptr nextFreeFast(span* s)
{
	int theBit = ctz64(s->allocCache);
	//因为alloccahe总共64位 检查是否还有空余即可
	if( theBit < 64 ) 
	{
		//result表示指向的是第几个空闲slot内存位
		uintptr result = s->freeindex + (uintptr)(theBit);
		//看看是否有超出span
		if( result < s->nelems ){
			uintptr freeidx = result + 1;
			//如果是64的倍数
			if( freeidx%64 == 0 && freeidx != s->nelems ){
				return 0;
			}
			//到这里说明成功的找到了一个可以分配的内存，更新对应的allocacache 和 freeindex
			s->allocCache >>= (uint)(theBit + 1);
			s->freeindex = freeidx;
			s->allocCount++;
			//根据start+addr  + size*pos 获取地址
			return (uintptr)(result*s->elemsize + s->startaddr);
		}
	}
	return 0;
}

//小对象 直接走每个P的缓存空闲链表上分配
//大于32kb的直接从全局heap堆上分配，(需要加锁)
void* mallocgc(uintptr size,type* typ,bool needzero)
{

	//这里应该是gc清除阶段，表示标记完成了，接下来就是清除，是不允许分配内存的
	if( gcphase != _GCoff){
	    sighandler(0);
//	if( gcphase == _GCmarktermination ){

//		throw("mallocgc called with gcphase == _GCmarktermination")
	}

	//asstG表示为当前分配管理的协程G,如果gc还没有启动则为NULL
	g* assistG;
	//不为0 说明已经处于gc的某个阶段了
	//标记辅助 #
	if( gcBlackenEnabled != 0 )
	{
		assistG = getg();
		if( assistG->m->curg != NULL ){
			assistG = assistG->m->curg;
		}
		// Charge the allocation against the G. We'll account
		// for internal fragmentation at the end of mallocgc.
		assistG->gcAssistBytes -= (int64)size;

		//TODO: gc 借债系统
//		if( assistG->gcAssistBytes < 0 ){
//			G欠债了，需要借债
			//通过借债和还债来降低gc的压力
//			gc->GcAssistAlloc(assistG);
//		}
	}

	// Set mp.mallocing to keep from being preempted by GC-> 设置一个标志位，防止被gc抢占
	m* mp = acquirem();
	if( mp->mallocing != 0 ){ //表示有死锁产生
		throw("malloc deadlock")
	}
	//如果当前m绑定的信号处理协程 == 当前malloc的协程 则说明在信号处理期间有内存分配的异常情况
	if( mp->gsignal == getg() ){
		throw("malloc during signal")
	}
	//设置mp.mallocing = 1 表示当前线程m正在进行 mallocing
	//应该是防止被抢占，因为某个协程在分配内存的时候是不能够进行抢占的
	mp->mallocing = 1;

	bool shouldhelpgc = false;
	uintptr dataSize  = size;
	
	//获取当前线程m绑定的 mcache
	cache* c = getg()->m->mcache;
	void* x;
	//这里就是分配的重要的一个分支，辨别当前要分配的对象是否在后续gc扫描对象的时候需要进一步扫描引用
	bool noscan = typ == NULL || (typ->kind & (1 << 7)) != 0;
	if( size <= maxSmallSize ){ //小于32k走默认内存分配
		//微对象分配
		if( noscan && size < maxTinySize ){
			uintptr off = c->tinyoffset;
			//对齐指针
			if( size&7 == 0 ) {
				off = round(off, 8);
			} else if( size&3 == 0 ){
				off = round(off, 4);
			} else if( size&1 == 0 ){
				off = round(off, 2);
			}
			if( off+size <= maxTinySize && c->tiny != 0 ){
				// The object fits into existing tiny block->
				x = (void*)(c->tiny + off);
				//记录以及分配的位置
				c->tinyoffset = off + size;
				//记录分配的对象总数
				c->local_tinyallocs++;
				mp->mallocing = 0;
				releasem(mp);
				return x;
			}
			//小对象不够用了
			span* s = c->alloc[tinySpanClass];
			//找到对应size的span列表
			uintptr v = nextFreeFast(s);

			//从mcache上分配失败了，接下来可能需要去 mcentral 和 heap 上加锁分配内存
			if( v == 0 ) {
			    //获取一个对象
				v = cache_nextFree(c,tinySpanClass,&s,&shouldhelpgc);
			}
			x = (void*)v;
			//进行16字节清0？
			((uint64*)x)[0] = 0;
			((uint64*)x)[1] = 0;
			// See if we need to replace the existing tiny block with the new one
			// based on amount of remaining free space->
			if( size < c->tinyoffset || c->tiny == 0 ){
				c->tiny = x;
				c->tinyoffset = size;
			}
			size = maxTinySize;
		} else {
		    uint8 sz;
			//计算出来需要多少size大小
			if( size <= smallSizeMax-8 ){
				sz = size_to_class8[(size+smallSizeDiv-1)/smallSizeDiv];
			} else {
				sz = size_to_class128[(size-smallSizeMax+largeSizeDiv-1)/largeSizeDiv];
			}
			size = (uintptr)(class_to_size[sz]);
			uint8 spc = makeSpanClass(sz, noscan);
			span* s = c->alloc[spc];
			//在去 寻找一个可用的span
			uintptr v = nextFreeFast(s);
			if( v == 0 ){
				v = cache_nextFree(c,spc,&s,&shouldhelpgc);
			}
			x = (void*)v;
			//看看是否需要清0
			if( needzero && s->needzero != 0 ){
				memset(v,0,size);
			}
		}
	} else {
		// 大于32k则走 heap 直接分配
		span* s;
		shouldhelpgc = true;
		s = largeAlloc(size,needzero,noscan);
		s->freeindex = 1;
		s->allocCount = 1;
		x = s->startaddr;
		size = s->elemsize;
	}

	uintptr scanSize;

	//说明内存里存在引用，需要进行扫描
	if( !noscan ){
	    //bit_heapBitsSetType(x,size,dataSize,typ);
		//如果申请的内存大于类型的大小
		if( dataSize > typ->size ){
            if( typ->ptrdata != 0 ){
				scanSize = dataSize - typ->size + typ->ptrdata;
			}
		} else {
			scanSize = typ->ptrdata;
		}
		c->local_scan += scanSize;
	}

	//如果处于gc阶段中，需要将所有新产生的内存自动标记为黑色
	if( gcphase != _GCoff ){
	    gc_marknewobject(x, size, scanSize);
	}

	mp->mallocing = 0; // unlock m
    //恢复线程m的相关状态
	releasem(mp);

	if( assistG != NULL ){
		// Account for internal fragmentation in the assist
		// debt now that we know it.
//		assistG->gcAssistBytes -= (int64)size - dataSize;
	}

	//需要触发一次gc ,多个线程并发gc只有一个线程会拿到锁后执行gc，其他线程只会帮忙处理一下清除工作
	if( shouldhelpgc ){
	    gcStart();
	}
	return x;
}
