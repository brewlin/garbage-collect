/**
 *@ClassName gc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/1 0001 下午 3:39
 *@Version 1.0
 **/
#include "sys/gc.h"
#include <unistd.h>
#include "malloc/malloc.h"
#include <pthread.h>
#include "sys/gpm.h"
#include "heap/heap.h"
#include "proc/proc.h"
#include "sys/defines.h"
#include "mgc/gc.h"
#include "mgc/root.h"


void initm(int _mid)
{
    stk_start = get_sp();
    regsig();
    _g_ = malloc(sizeof(g));
    _g_->m = malloc(sizeof(m));
    _g_->m->mallocing  = 0;
    _g_->m->mcache = allocmcache();
    mid = _mid;
    _g_->m->mid = _mid;
    _g_->m->pid = pthread_self();

}
void* alloc(void* arg){
    //每个线程初始化本地tls
    initm(arg);
    allm[(int)arg] = getg()->m;
    while(gcphase != _GCoff){}


    int n = 100;
    //构造一个需要gc扫描的内存
    type typ;
    typ.kind = 1;
    int* arr = mallocgc(n* sizeof(int),&typ,true);
    for(int i = 0; i< n ;i ++){
        //干扰信息
        int interfere = rand()%90;
        mallocgc(interfere,NULL,true);
        arr[i] = i;
    }
    for(int i = 0; i < n ; i ++){
        if(arr[i] != i) throw("mem error!")
    }


}
void osinit()
{
    ncpu = (int32)(sysconf(_SC_NPROCESSORS_ONLN));
    physPageSize = sysconf(_SC_PAGESIZE);  // 出于通用性的考虑， 物理页大小获取的方式是通过 POSIX sysctl 这个系统调用进行获取
    gcphase = _GCoff;
    gcBlackenEnabled = false;
    //初始化全局堆, 对应 func mallocinit()
    mallocinit();
    //TODO: 开启多线程进行并发gc需要调度器参与，目前只支持单线程gc，但支持多线程分配
    pthread_t tid;
    pthread_create(&tid,NULL,alloc,0);
    pthread_join(tid,NULL);
}
void main(){
    DEBUG("start");
    osinit(); // 对应golang src/runtime/os_linux.go func osinit() osinit 完成对 CPU 核心数的获取，因为这与调度器有关, 运行时最为重要的两个系统级参数：CPU 核心数与内存物理页大小
    DEBUG("end");
}
