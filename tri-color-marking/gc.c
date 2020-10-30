#include "gc.h"

//回收链表，挂着空闲链表
Header *free_list = NULL;
GC_Heap gc_heaps[HEAP_LIMIT];
size_t gc_heaps_used = 0;
int auto_gc  = 1;
int auto_grow  = 1;
int gc_phase = GC_ROOT_SCAN;
int max_mark = 0;
int max_sweep = 0;


void gc_init(size_t heap_size,size_t count)
{
    gc_heaps_used = count;
    for (size_t i = 0; i < count; i++){
        //使用sbrk 向操作系统申请大内存块
        void* p = sbrk(heap_size + PTRSIZE + HEADER_SIZE);
        if(p == NULL)exit(-1);

        gc_heaps[i].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
        gc_heaps[i].size = heap_size;
        gc_heaps[i].slot->size = heap_size;
        gc_heaps[i].slot->next_free = NULL;
        gc_free((Header*)p + 1);
    }

}
/**
 * 增加堆
 **/
Header* add_heap(size_t req_size)
{
    void *p;
    Header *align_p;
    //超过堆大小后 内存耗光 告警退出
    if (gc_heaps_used >= HEAP_LIMIT) {
        fputs("OutOfMemory Error", stderr);
        abort();
    }

    //申请的堆大小 不能小于 0x4000
    if (req_size < TINY_HEAP_SIZE)
        req_size = TINY_HEAP_SIZE;

    //使用sbrk 向操作系统申请大内存块
    // size + header大小 最后加一个 8字节是为了内存对齐
    if((p = sbrk(req_size + PTRSIZE + HEADER_SIZE)) == (void *)-1){
        DEBUG(printf("sbrk 分配内存失败\n"));
        return NULL;
    }

    /* address alignment */
    //地址对齐
    align_p = gc_heaps[gc_heaps_used].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
    req_size = gc_heaps[gc_heaps_used].size = req_size;
    align_p->size = req_size;
    //新的堆的下一个节点依然指向本身
    align_p->next_free = NULL;
    //新增一个堆
    gc_heaps_used++;
    DEBUG(printf("扩堆内存:%ld ptr:%p\n",req_size,align_p));

    return align_p;
}
/**
 * 扩充堆大小
 **/
Header* grow(size_t req_size)
{
    Header *cp, *up;

    if (!(cp = add_heap(req_size))){
        printf("扩充堆 大小失败");
        return NULL;
    }

    up = (Header *) cp;
    printf("%p\n",up);
    //因为gc_free 是公用的，所以需要 将实际 mem 地址传入
    //新申请的堆不需要 进行gc 只需要挂载到 free_list 链表上就行
    gc_free((void *)(up+1));
    //上面执行gc的时候，其实已经将free_list的表头改变，
    // 所以free_list->next_free 可能就是 up
    return free_list;
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
            }
            p->size = req_size;
            free_list = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            FL_SET(p, FL_ALLOC);
            printf("%p\n",p);
            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }

    }

    //这里表示前面多次都没有找到合适的空间，且已经遍历完了空闲链表 free_list
    //这里表示在 单次内存申请的时候 且 空间不够用的情况下 需要执行一次gc
    //一般是分块用尽会 才会执行gc 清除带回收的内存
    if (!do_gc && auto_gc) {
        gc();
        do_gc = 1;
        goto alloc;
    }else if(auto_grow){
        //上面说明 执行了gc之后 内存依然不够用 那么需要扩充堆大小
        p = grow(req_size);
        if(p != NULL) goto alloc;
    }
    printf("分配失败，内存不够\n");
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
//    free_list = hit;
    target->flags = 0;
}

//记录目前的堆size
// size_t   root_ranges_used = 0;
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
Header*  get_header(GC_Heap *gh, void *ptr)
{
    Header *p, *pend, *pnext;

    pend = (Header *)(((size_t)gh->slot) + gh->size);
    for (p = gh->slot; p < pend; p = pnext) {
        pnext = NEXT_HEADER(p);
        if ((void *)(p+1) <= ptr && ptr < (void *)pnext) {
            return p;
        }
    }
    return NULL;
}