/**
 *@ClassName fixalloc
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 3:48
 *@Version 1.0
 **/

#include "fixalloc.h"
#include "../sys/sysapi.h"

palloc globalAlloc;
mutex  ga_lock;
void*  persistentChunks;
/**
 *
 * @param size
 * @param func
 */
void fixalloc_init(fixalloc* f,uintptr size,firstp first,void* arg){
    f->size  = size;
    f->first = first;

    f->arg   = arg;
    f->list  = NULL;
    f->chunk = 0;
    f->nchunk = 0;
    f->inuse = 0;
    f->zero  = true;

}
/**
 *
 * @param f
 * @param p
 */
void fixalloc_free(fixalloc* f,uintptr*  p)
{
    f->inuse -= f->size;
    mlink* v = (mlink*)p;
    //插入链表头 O(1);
    v->next  = f->list;
    f->list  = v;
}

/**
 * 固定分配f->size
 * @param f
 * @return
 */
uintptr* fixalloc_alloc(fixalloc* f)
{
    void *v;
    if (f->size == 0 ){
        printf("runtime: use of FixAlloc_Alloc before FixAlloc_Init\n");
        printf("runtime: internal error\n");
        exit(-1);
    }
    
    if (f->list != NULL ) {
        v = (void*)f->list;
        f->list = f->list->next;
        f->inuse += f->size;
        if (f->zero ) {
            memset(v,0,f->size);
        }
        return v;
    }
    if ((uintptr)f->nchunk < f->size ) {
        //内存不够了
        f->chunk = (uintptr)sys_alloc(fixAllocChunk);
        f->nchunk = fixAllocChunk;
    }
    
    v = (void*)f->chunk;
    //只有span 分配的时候才会有这个钩子
    if (f->first != NULL ) {
        f->first(f->arg, v);
    }
    f->chunk  =  f->chunk + f->size;
    f->nchunk -= (uint32)(f->size);
    f->inuse  += f->size;
    return v;
}