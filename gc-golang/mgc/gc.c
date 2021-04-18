#include "../sys/gc.h"
#include "../sys/gpm.h"
#include "gc.h"
#include "../atomic/atomic.h"
#include "gc_work.h"
#include "../heap/heap.h"
#include "bitmap.h"
#include "../proc/proc.h"

mutex worldsema;
/**
 * 开始gc，将状态gcoff 转换为 gcmark阶段
 * @return
 */
void gcStart()
{
	// 多个线程并发的获取 剩余未清除的span,进行垃圾清除
	// 防止gc跨代了
	while(gcphase == _GCoff && sweepone() != ~(uintptr)0){}
	if(work.gcing > 0) {
	    sighandler(0);
	    return;
	}

    lock(&worldsema);
    atomic_Xadd(&work.gcing,1);

	if(gcphase != _GCoff){
	    unlock(&worldsema);
        atomic_Xadd(&work.gcing,-1);
	    return;
	}
    // 进入并发标记阶段，开启写屏障
    gcphase = _GCmark;
    work.userForced = true;
	stopTheWorld(&allm,ncpu);
	//开始标记
	gcWork gcw;
	gcwork_init(&gcw);
    gcDrain(&gcw);
    //标记结束
    gcMarkDone();
    startTheWorld();
//    atomic_Xadd(&work.gcing,-1);
    work.gcing = 0;
    unlock(&worldsema);

}
/**
 * 标记阶段结束 ——> 标记终止状态
 * 所有可达对象都标记为黑色对象，无灰色对象存在
 * 当前函数在多线程下只有最后一个标记线程会触发调用(所有线程都陷入等待，那么表明所有标记任务完成) work->nwait == work->nproc && !gcMarkWorkAvailable(p)
 * @return
 */
void gcMarkDone()
{
	//FIXME: 确保只有一个线程能进入这里	semacquire(&work->markDoneSema);
	if( gcphase != _GCmark ){
	//	semrelease(&work->markDoneSema);
		return;
	}
	// Perform mark termination. This will restart the world.
	gcMarkTermination();
}

/**
 * 同样只有一个线程会触发当前方法
 * 标记阶段转移为标记结束阶段
 * 标记终止  -> gc终止状态
 * @param float64
 * @return
 */
void gcMarkTermination()
{
	// World is stopped.
	// Start marktermination which includes enabling the write barrier.
	atomic_Store(&gcBlackenEnabled, false);
	atomic_Store(&gcphase,_GCmarktermination);
	//转换为_GCoff阶段
	atomic_Store(&gcphase,_GCoff);
	gcSweep();

	for(int i = 0; i < ncpu ; i ++){
		if(allm[i] == NULL)continue;
		cache_releaseAll(allm[i]->mcache);
	}
}

/**
 * 当前gc阶段完全结束，开始关闭写屏障
 * 将当前全局 sweepgen += 2 结束当前轮
 * 注意： 这里的结束并非是清除结束，清除阶段是有一些惰性的操作存在
 * 如果是用户层面调用的gc，则需要立即清除全部
 * @param gcMode
 * @return
 */
void gcSweep()
{
    if( gcphase != _GCoff ){
        throw("gcSweep being done but phase is not GCoff")
    }

    lock(&heap_.locks);
    //gc标志+2，和 span的sweepgen 相互影响
    heap_.sweepgen += 2;
    heap_.sweepdone = 0;
	if(  heap_.sweepSpans[heap_.sweepgen/2%2].index != 0 ){
		throw("non-empty swept list")
	}
    unlock(&heap_.locks);

    //如果是阻塞 gc模式 需要立即清扫
    if(work.userForced){
        // Sweep all spans eagerly.
        while( sweepone() != ~(uintptr)0 ){}
        //标记结束，可以关闭写屏障了
    }
	return;

}

/**
 * 从队列里选取一个进行清理，在多线程下会被多个线程竞争进行并发清理
 * 采用原子操作，优化锁竞争
 * @return
 */
uintptr sweepone()
{
	g* _g_ = getg();
	if( heap_.sweepdone != 0 ){
		return ~(uintptr)0;
	}
	atomic_Xadd(&heap_.sweepers, 1);

	span* s ;
	uint32 sg = heap_.sweepgen;
	while(true){
		s = gcsweepbuf_pop(&heap_.sweepSpans[1-sg/2%2]);
		//所有的span都清理完毕
		if( s == NULL ){
			atomic_Store(&heap_.sweepdone, 1);
			break;
		}
		if( s->state != mSpanInUse ){
		    //如果清扫已经清扫过的对象 就会发送一次
			if( !(s->sweepgen == sg || s->sweepgen == sg+3) ){
				throw("non in-use span in unswept list");
			}
			continue;
		}
		//找到了一个需要清理的span，手动修改标志位为正在清理中
		if( s->sweepgen == sg-2 && atomic_Cas(&s->sweepgen, sg-2, sg-1) ){
			break;
		}
	}

	//清理我们找到的span
	uintptr npages = ~(uintptr)0;
	if( s != NULL ){
		npages = s->npages;
		if( span_sweep(s,false)){
		    //被释放了
		} else {
    		//span任然在使用
			npages = 0;
		}
	}
	atomic_Xadd(&heap_.sweepers, -1);
	return npages;
}

/**
 * 需要运行在独立的协程或者线程中,不停的唤醒-睡眠
 */
void bgsweep()
{
//	sweep.g = getg();

//	lock(&sweep.lock)
//	sweep.parked = true
//	goparkunlock(&sweep.lock, waitReasonGCSweepWait, traceEvGoBlock, 1)

	while(true){
		//每清除一个span，就主动让出一下线程
		while( sweepone() != ~(uintptr)0 ){}
	}
}