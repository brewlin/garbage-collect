
/**
 *@ClassName gpm
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/30 0030 下午 3:44
 *@Version 1.0
 **/
#ifndef GOC_GPM_H
#define GOC_GPM_H

#include "gc.h"
#include "../cache/cache.h"
#include "../malloc/fixalloc.h"
#include "array.h"

typedef struct mstack stack;
typedef struct gg     g;
typedef struct mm     m;
typedef struct pp	  p;

struct mstack {
	uintptr	lo;
	uintptr hi;
};
//协程的主要结构体，通过go() => newproces() new 一个g的结构体
struct gg {
    m* 		    m;// 每个协程g所属的线程m  current m; offset known to arm liblink
	//抢占信号，检测是否被抢占
	bool		preempt;
	//记录了协程栈的信息
	stack       stk;    // offset known to runtime/cgo //这个地方记录了当前协程的栈空间信息
	uintptr     stackguard0; // offset known to liblink
	uintptr 	stackguard1; // offset known to liblink
	//标记辅助 #
	// 为了保证用户程序分配内存的速度不会超出后台任务的标记速度，运行时还引入了标记辅助技术，
	// 它遵循一条非常简单并且朴实的原则，分配多少内存就需要完成多少标记任务。
	// 每一个 Goroutine 都持有 gcAssistBytes 字段，这个字段存储了当前 Goroutine 辅助标记的对象字节数。
	// 在并发标记阶段期间，当 Goroutine 调用 runtime.mallocgc 分配新对象时，
	// 该函数会检查申请内存的 Goroutine 是否处于入不敷出的状态：
	int64 		gcAssistBytes;
	mutex 		locks;
};

//一个m表示一个ilnux线程，在启动时通过go.maxproces指定最大的线程，目前版本已经没有最大线程的上限了
struct mm {
    uintptr  pid;
    uint     mid;
	//特殊的协程，每个P固定绑定有一个G，负责配合管理调度
	g* 		 g0;
	//每个m绑定了一个本地mcache
	cache*	 mcache;
	p*	 	 p;
	//简单的锁计数
	mutex 	 locks;
	//当前正在运行的G
	g*		 curg;
	//如果某个M在进行分配内存工作那么这个线程是不可以被抢占的
	int32    mallocing;
	//信号处理的协程
	g*		 gsignal;

	uint32	 fastrand[2];

};

//GPM 中的p 管理者协程队列
struct pp {
	//每个p自带一个 固定内存分配
	palloc 	pl;

	//当前P的状态
	uint32	status; //当前p的状态 one of pidle/prunning/...
	//绑定对应线程M
	uintptr m;

	//每个处理器都标配一个特殊协程，专用于gc阶段的标记工作
	uintptr gcBgMarkWorker;

};


m* 	 acquirem();
void releasem(m* m);
g*   getg();
extern __thread  g* _g_;
extern m*		 allm[10];
#endif //GOC_GPM_H
