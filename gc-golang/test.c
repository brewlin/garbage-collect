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


void initm(int _mid)
{
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

    int i = 0;
    while(i < 1000){
        int size = rand() % 90;
        int *p = mallocgc(size,NULL,true);
        if(p == NULL){
            throw("alloc failed\n")
        }
        *p = i;
        i++;
    }
}
void osinit()
{
    ncpu = (int32)(sysconf(_SC_NPROCESSORS_ONLN));
    physPageSize = sysconf(_SC_PAGESIZE);
    gcphase = _GCoff;
    gcBlackenEnabled = false;
    //初始化全局堆
    mallocinit();
    //TODO: 开启多个线程进行分配内存,并发gc需要调度器参与，目前只支持单线程gc，但支持多线程分配
    pthread_t tid;
    pthread_create(&tid,NULL,alloc,0);
    pthread_join(tid,NULL);
}
void main(){
    osinit();
}
