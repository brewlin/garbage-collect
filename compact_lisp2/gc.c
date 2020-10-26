#include "gc.h"

//回收链表，挂着空闲链表
Header *free_list = NULL;
GC_Heap gc_heaps[10];
int     gc_heaps_used;


/**
 * 初始化所有的堆
 **/
void gc_init(size_t heap_size)
{
    gc_heaps_used = 1;
    //使用sbrk 向操作系统申请大内存块
    void* p = sbrk(heap_size + PTRSIZE + HEADER_SIZE);
    gc_heaps[0].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
    gc_heaps[0].size = heap_size;
    gc_heaps[0].slot->size = heap_size;
    gc_heaps[0].slot->next_free = NULL;
    //将堆初始化到free_list 链表上
    gc_free(gc_heaps[0].slot + 1);
}
/**
 * 分配一块内存
 */
void*   gc_malloc(size_t req_size)
{
    DEBUG(printf("内存申请 :%ld\n",req_size));
    Header *p, *prevp;
    size_t do_gc = 0;
    //对齐 字节
    req_size = ALIGN(req_size, PTRSIZE);

    if (req_size <= 0) {
        return NULL;
    }
alloc:
    //从空闲链表上去搜寻 空余空间
    prevp = free_list;
    //死循环 遍历
    for (p = prevp; p; prevp = p, p = p->next_free) {
        //堆的内存足够
        if (p->size >= req_size + HEADER_SIZE) {
            //刚好满足
            if (p->size == req_size + HEADER_SIZE)
                /* 刚好满足 */
                // 从空闲列表上 移除当前的 堆，因为申请的大小刚好把堆消耗完了
                if(p == prevp)
                    prevp = p->next_free;
                else
                    prevp->next_free = p->next_free;

            //没有刚好相同的空间，所以从大分块中拆分一块出来给用户
            //这里因为有拆分 所以会导致内存碎片的问题，这也是 标记清除算法的一个缺点
            //就是导致内存碎片
            else {
                //TODO: 这里采用从头分配法，其他gc算法也需要测试一下这种情况
//                p->size -= (req_size + HEADER_SIZE);
//                p = NEXT_HEADER(p);
                //这里就是从当前堆的堆首  跳转到末尾申请的那个堆
                prevp = (void*)prevp + HEADER_SIZE + req_size;
                prevp->size = p->size - (req_size + HEADER_SIZE);
            }
            p->size   = req_size;
            free_list = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            FL_SET(p, FL_ALLOC);
            printf("%p\n",p);
            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }
    }
    //一般是分块用尽会 才会执行gc 清除带回收的内存
    if (!do_gc) {
        do_gc = 1;
        //内存不够用的时候会触发 复制 释放空间
        //释放空间的时候会造成空间的压缩
        gc();
        goto alloc;
    }
    printf("内存不够\n");
    return NULL;

}
/**
 * 传入的是一个内存地址 是不带header头的
 * 所以我们可以通过（Header*)ptr - 1 来定位到header头
 **/
void    gc_free(void *ptr)
{
    DEBUG(printf("释放内存 :%p free_list:%p\n",ptr,free_list));
    Header *target, *hit;
    //通过内存地址向上偏移量找到  header头
    target = (Header *)ptr - 1;
    //回收的数据立马清空
    memset(ptr,0,target->size);

    if(free_list == NULL){
        free_list = target;
        target->flags = 0;
        return;
    }
    /* search join point of target to free_list */
    for (hit = free_list; !(target > hit && target < hit->next_free); hit = hit->next_free)
        /* heap end? And hit(search)? */
        if (hit >= hit->next_free && (target > hit || target < hit->next_free || hit->next_free == NULL))
            break;

    // 1. 在扩充堆的时候 这个target 的下个header 指向的是非法空间
    // 在空闲链表上 找到了当前header
    if (NEXT_HEADER(target) == hit->next_free) {
        /* merge */
        target->size += (hit->next_free->size + HEADER_SIZE);
        target->next_free = hit->next_free->next_free;
    }else {
        /* join next free block */
        //1. 在扩充堆的时候 新生成的堆 会插入到 free_list 后面
        target->next_free = hit->next_free;
    }
    //如果当前待回收的内存 属于 hit堆里的一部分，则进行合并
    if (NEXT_HEADER(hit) == target) {
        /* merge */
        hit->size += (target->size + HEADER_SIZE);
        hit->next_free = target->next_free;
    }else {
        //如果不是则 直接挂到空闲链表后面
        /* join before free block */
        hit->next_free = target;
    }
    free_list = hit;
    target->flags = 0;
}
GC_Heap* hit_cache = NULL;
GC_Heap* is_pointer_to_heap(void *ptr)
{
    size_t i;

    if (hit_cache &&
        ((void *)hit_cache->slot) <= ptr &&
        (size_t)ptr < (((size_t)hit_cache->slot) + hit_cache->size))
        return hit_cache;
    for (i = 0; i < gc_heaps_used;  i++) {
        if ((((void *)gc_heaps[i].slot) <= ptr) &&
            ((size_t)ptr < (((size_t)gc_heaps[i].slot) + HEADER_SIZE +  gc_heaps[i].size))) {
            hit_cache = &gc_heaps[i];
            return &gc_heaps[i];
        }
    }
    return NULL;
}
Header*  get_header(void *ptr)
{
    GC_Heap* gh = is_pointer_to_heap(ptr);
    if(!gh) return NULL;
    
    Header* hp = gh->slot;
    if ((((void *)hp) > ptr) && ((((void*)hp) + HEADER_SIZE +  gh->size)) <= ptr) {
        return NULL;
    }
    Header *p, *pend, *pnext;
    pend = (Header *)(((void*)(hp+1)) + gh->size);
    for (p = hp; p < pend; p = pnext) {
        pnext = NEXT_HEADER(p);
        if ((void *)(p+1) <= ptr && ptr < (void *)pnext) {
            return p;
        }
    }
    return NULL;
}

