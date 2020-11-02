#include "gc.h"

//多个回收链表 索引0 表示大于100字节的大内存空闲链表
//1 - 99 分别表示各个字节的空闲内存
Header *free_list[100];

GC_Heap gc_heaps[HEAP_LIMIT];
size_t gc_heaps_used = 0;
int auto_gc = 1;


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

    //申请的堆大小 不能大于99 则 默认分配0x4000 大内存块 共享
    if (req_size > MAX_SLICE_HEAP){
        if(req_size < TINY_HEAP_SIZE)
            req_size = TINY_HEAP_SIZE;
        printf("大内存分配:%d\n",req_size);
        //使用sbrk 向操作系统申请大内存块
        // size + header大小 最后加一个 8字节是为了内存对齐
        if((p = sbrk(req_size + PTRSIZE + HEADER_SIZE)) == (void *)-1){
            printf("sbrk 分配内存失败\n");
            return NULL;
        }
    //小内存块
    }else{
        printf("小内存分配:%d\n",req_size);
        if((p = malloc(req_size + HEADER_SIZE + PTRSIZE)) == (void*)-1){
            printf("mallock 分配内存失败\n");
            return NULL;
        }

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
    //因为gc_free 是公用的，所以需要 将实际 mem 地址传入
    //新申请的堆不需要 进行gc 只需要挂载到 free_list 链表上就行
    gc_free((void *)(up+1));
    //上面执行gc的时候，其实已经将free_list的表头改变，
    // 所以free_list->next_free 可能就是 up
    if (req_size > MAX_SLICE_HEAP)
        return free_list[0];
    else
        return free_list[req_size];
}
/**
 * 分配一块内存
 */
void*   gc_malloc(size_t req_size)
{
    printf("gc_malloc :%ld\n",req_size);
    Header *p, *prevp;
    size_t do_gc = 0;
    //对齐 字节
    req_size = ALIGN(req_size, PTRSIZE);
    int index = req_size;
    if (req_size <= 0) {
        return NULL;
    }
    if(req_size > MAX_SLICE_HEAP)
        index = 0;
    printf("gc_malloc :%d size:%d\n",index,req_size);

    alloc:
    //从空闲链表上去搜寻 空余空间
    prevp = free_list[index];
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
                //从当前堆的末尾 减去 （req_seize+ header头）的空间返回给用户用
                p->size -= (req_size + HEADER_SIZE);
                //这里就是从当前堆的堆首  跳转到末尾申请的那个堆
                p = NEXT_HEADER(p);
                p->size = req_size;
            }
            free_list[index] = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            FL_SET(p, FL_ALLOC);
            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }
    }
    //这里表示前面多次都没有找到合适的空间，且已经遍历完了空闲链表 free_list
    //这里表示在 单次内存申请的时候 且 空间不够用的情况下 需要执行一次gc
    if (!do_gc && auto_gc) {
        gc();
        do_gc = 1;
        goto alloc;
    }
        //上面说明 执行了gc之后 内存依然不够用 那么需要扩充堆大小
    else if ((p = grow(req_size)) != NULL){
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
    DEBUG(printf("start free mem:%p\n",ptr));
    Header *target, *hit;
    //通过内存地址向上偏移量找到  header头
    target = (Header *)ptr - 1;
    //回收的数据立马清空
    memset(ptr,0,target->size);

    //如果是小内存 不需要合并直接挂到最新的表头即可
    if(target->size <= MAX_SLICE_HEAP){
        if(free_list[target->size]){
            target->next_free = free_list[target->size]->next_free;
            free_list[target->size]->next_free = target;
        }else{
            free_list[target->size] = target;
        }
        return;
    }
    //大内存
    if(free_list[0] == NULL){
        free_list[0] = target;
        target->flags = 0;
        return;
    }
    /* search join point of target to free_list */
    //在回收的时候这个步骤 是为了找到 当前地址所在堆，但是如果当前ptr为新增的堆 则不需要这个步骤
    for (hit = free_list[0]; !(target > hit && target < hit->next_free); hit = hit->next_free)
        if (hit >= hit->next_free && (target > hit || target < hit->next_free || hit->next_free == NULL))
            break;

    // 1. 在扩充堆的时候 这个target 的下个header 指向的是非法空间
    // 在空闲链表上 找到了当前header
    if (NEXT_HEADER(target) == hit->next_free) {
        /* merge */
        // target hit   hit->next
        //将hit 合并到 target =  target,hit->next
        target->size += (hit->next_free->size + HEADER_SIZE);
        target->next_free = hit->next_free->next_free;
    }else {
        /* join next free block */
        //1. 在扩充堆的时候 新生成的堆 会插入到 free_list 后面
        //target->next_free = free_list->next_free
        //free_list = target
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
//    free_list[0] = hit;
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