#include "gc.h"

//回收链表，挂着空闲链表
Header *free_list = NULL;
GC_Heap gc_heaps[10];
int     gc_heaps_used;
int from = 1;
int to   = 0;

//生成空间 new generational
int newg          = 0;
//幸存空间1 survivor from generational
int survivorfromg = 1;
//幸存空间2 survivor to generational
int survivortog   = 2;
//老年代空间 old generational
int oldg          = 3;

Header* new_free_p;

/**
 * 初始化 4个堆
 * 1个生成空间
 * 2个幸存空间
 * 1个老年代空间
 **/
void gc_init(size_t heap_size)
{
    //gc_heaps[3] 用于老年代堆区
    gc_heaps_used = 3;
    for (size_t i = 0; i < 4; i++){
        //使用sbrk 向操作系统申请大内存块
        void* p = sbrk(heap_size + PTRSIZE + HEADER_SIZE);
        if(p == NULL)exit(-1);

        gc_heaps[i].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
        gc_heaps[i].size = heap_size;
        gc_heaps[i].slot->size = heap_size;
        gc_heaps[i].slot->next_free = NULL;
        gc_heaps[i].end  = (void*)gc_heaps[i].slot + heap_size + HEADER_SIZE;
    }
    //初始化新生代空闲链表指针
    new_free_p = gc_heaps[newg].slot;
    //老年代分配需要用到空闲列表 通过gc_free 挂到空闲列表即可
    gc_free(gc_heaps[oldg].slot + 1);
}
/**
 * 新生代分配 不需要用到空闲链表 直接根据 free_p 来向后进行分配
 */
void*   gc_malloc(size_t req_size)
{
    DEBUG(printf("内存申请 :%ld\n",req_size));
    Header *obj;
    size_t do_gc = 0;

    //对齐 字节
    req_size = ALIGN(req_size, PTRSIZE);
    if (req_size <= 0) return NULL;

    alloc:
    //查看分配空间是否还够用
    if (new_free_p->size < req_size + HEADER_SIZE) {
        //一般是分块用尽会 才会执行gc 清除带回收的内存
        if (!do_gc) {
            do_gc = 1;
            //内存不够用的时候会触发 复制 释放空间
            //释放空间的时候会造成空间的压缩
            gc();
            goto alloc;
        }
        printf("内存不够")
        return NULL;
    }
    obj = free_list;
    obj->size = req_size;
    FL_SET(obj, FL_ALLOC);
    //设置年龄为0
    obj->age = 0;
    obj->forwarding = NULL;
    FL_UNSET(obj,FL_REMEMBERED);

    //将新生代空闲指针 移动到下一个空闲地址开头
    int left_size = new_free_p->size - HEADER_SIZE - req_size;
    new_free_p = (void*)(new_free_p+1) + req_size;
    //并设置正确的空余空间大小
    new_free_p->size = left_size;

    return obj+1;
}
/**
 * 老年代分配
 * 老年代分配走 标记清除算法，所以需要用到空闲链表
 */
void*   gc_old_malloc(size_t req_size)
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
                p->size -= (req_size + HEADER_SIZE);
                //这里就是从当前堆的堆首  跳转到末尾申请的那个堆
                p = NEXT_HEADER(p);
            }
            p->size   = req_size;
            free_list = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            FL_SET(p, FL_ALLOC);
            FL_UNSET(p,FL_COPIED);
            //设置年龄为0
            p->age = 0;
            p->forwarding = NULL;
            FL_UNSET(p,FL_REMEMBERED);

            printf("%p\n",p);
            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }
    }
    //一般是分块用尽会 才会执行gc 清除带回收的内存
    if (!do_gc) {
        do_gc = 1;
        //执行老年代gc
        major_gc();
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

//在gc的时候 from已经全部复制到to堆
//这个时候需要清空from堆，但是再次之前我们需要将free_list空闲指针还保留在from堆上的去除
void remove_from(){
    //遍历所有的空闲链表如果在from堆则去除该引用
    Header* prevp = free_list;

    void* from_start = gc_heaps[from].slot;
    void* from_end   = gc_heaps[from].slot + HEADER_SIZE + gc_heaps[from].size;
    for (Header* p = prevp; p ; prevp = p, p = p->next_free) {
        //看看当前 p 是否在堆  from上
        if((void*)p <= from_end && (void*)p >= from_start)
            prevp->next_free = p->next_free;
    } 

}