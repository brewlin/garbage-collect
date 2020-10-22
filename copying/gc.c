#include "gc.h"

//回收链表，挂着空闲链表
Header *free_list = NULL;
Header* from;
Header* to;


/**
 * 增加堆
 **/
void gc_init()
{
    req_size = TINY_HEAP_SIZE;
    //使用sbrk 向操作系统申请大内存块
    void* from_p = sbrk(req_size + PTRSIZE + HEADER_SIZE);
    from  = (Header *)ALIGN((size_t)from_p, PTRSIZE);
    from->size = req_size;
    from->next_free = NULL;
    gc_free((void*)from+1);
    DEBUG(printf("扩堆内存:%ld ptr:%p\n",req_size,align_p));

     //使用sbrk 向操作系统申请大内存块
    void* to_p = sbrk(req_size + PTRSIZE + HEADER_SIZE);
    to  = (Header *)ALIGN((size_t)to_p, PTRSIZE);
    to->size = req_size;
    to->next_free = NULL;
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
        if (p->size >= req_size) {
            //刚好满足
            if (p->size == req_size)
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
                /* too big */
                p->size -= (req_size + HEADER_SIZE);
                //这里就是从当前堆的堆首  跳转到末尾申请的那个堆
                p = NEXT_HEADER(p);
                p->size = req_size;
            }
            free_list = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            p->flags = 0;
            printf("%p\n",p);
            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }

    }

    //一般是分块用尽会 才会执行gc 清除带回收的内存
    if (!do_gc) {
        do_gc = 1;
        goto alloc;
    }
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
        if (hit >= hit->next_free && (target > hit || target < hit->next_free))
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


Header*  get_header(void *ptr)
{
    size_t i;
    if ((((void *)from) > ptr) && ((((void*)from) + HEADER_SIZE +  from->size)) <= ptr) {
        return NULL;
    }
    Header *p, *pend, *pnext;

    pend = (Header *)(((void*)from) + from->size);
    for (p = from; p < pend; p = pnext) {
        pnext = NEXT_HEADER(p);
        if ((void *)(p+1) <= ptr && ptr < (void *)pnext) {
            return p;
        }
    }
    return NULL;
}