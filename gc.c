#include "gc.h"
#include <time.h>

//回收链表，挂着空闲链表
Header* free_list = NULL;
GC_Heap gc_heaps[HEAP_LIMIT];
size_t  gc_heaps_used = 0;
root    roots[ROOT_RANGES_LIMIT]; //根
size_t  root_used = 0;
int     auto_gc = 1;
int     auto_grow = 1;


/**
 * 将分配的变量 添加到root 引用,只要是root上的对象都能够进行标记
 * @param start
 * @param end
 */
void add_roots(void* o_ptr)
{
    void *ptr = *(void**)o_ptr;
    roots[root_used].ptr = ptr;
    roots[root_used].optr = o_ptr;
    root_used++;
    if (root_used >= ROOT_RANGES_LIMIT) {
        fputs("Root OverFlow", stderr);
        abort();
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
    if((p = sbrk(req_size + PTRSIZE)) == (void *)-1){
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
Header* gc_grow(size_t req_size)
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
    if(free_list == NULL){
        memset(up +  1,0,up->size - HEADER_SIZE);
        free_list = up;
        up->flags = 0;
        return free_list;
    }else{
        gc_free((void *)(up+1));
    }
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

    req_size += HEADER_SIZE;
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
                    free_list = prevp = p->next_free;
                else
                    prevp->next_free = p->next_free;

            //没有刚好相同的空间，所以从大分块中拆分一块出来给用户
            //这里因为有拆分 所以会导致内存碎片的问题，这也是 标记清除算法的一个缺点
            //就是导致内存碎片
            else {
                /* too big */
//                p->size -= (req_size + HEADER_SIZE);
//                这里就是从当前堆的堆首  跳转到末尾申请的那个堆
//                p = NEXT_HEADER(p);
                prevp = (void*)prevp + req_size;
                memcpy(prevp,p,HEADER_SIZE);
                prevp->size = p->size - req_size;
            }
            p->size = req_size;
            free_list = prevp;
            //给新分配的p 设置为标志位 fl_alloc 为新分配的空间
            printf("%p\n",p);
            p->flags = 0;
            p->ref   = 1;
            FL_SET(p, FL_ALLOC);
            //设置年龄为0
            p->age = 0;
            p->forwarding = NULL;

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
    }else if(auto_grow){ //上面说明 执行了gc之后 内存依然不够用 那么需要扩充堆大小
        p = gc_grow(req_size);
        if(p != NULL) goto alloc;
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
    Header *target, *hit,*prevp;
    //通过内存地址向上偏移量找到  header头
    target = (Header *)ptr - 1;
    //回收的数据立马清空
    memset(ptr,0,target->size-HEADER_SIZE);
    target->flags = 0;

    //空闲链表为空，直接将当前target挂到上面
    if(free_list == NULL){
        free_list = target;
        target->flags = 0;
        return;
    }
    //特殊情况，如果target->next == free_list 在上面是无法判断的
    if(NEXT_HEADER(target) == free_list){
        target->size += (free_list->size);
        target->next_free = free_list->next_free;
        free_list = target;
        return;
    }
    //搜索target可能在空闲链表上的区间位置
    prevp = free_list;
    for(hit = prevp; hit && hit->next_free ; prevp = hit,hit = hit->next_free)
    {
        //刚好 target就在 [hit,hit->next_free] 之间
        if(target >= hit && target <= hit->next_free){
            break;
        }
        //跨堆的情况 说明target在两个堆之间 (heap1_end,heap2_start)
        if(hit >= hit->next_free && (target > hit || target < hit->next_free))
            break;
    }

    //1. 判断右区间  如果target属于右区间 则合并
    if (NEXT_HEADER(target) == hit->next_free) {
        target->size += hit->next_free->size;
        target->next_free = hit->next_free->next_free;
    }else {
        target->next_free = hit->next_free;
    }

    //2. 判断左区间  如果target属于左区间 则合并
    if (NEXT_HEADER(hit) == target) {
        /* merge */
        hit->size += target->size;
        hit->next_free = target->next_free;
    }else {
        hit->next_free = target;
    }
}


/**
 * 判断指针是否属于某个堆
 * 辨别指针 | 非指针
 * 保守式gc来说 会有一定误差
 * @param ptr
 * @return
 */
GC_Heap* is_pointer_to_heap(void *ptr)
{
    size_t i;

    for (i = 0; i < gc_heaps_used;  i++) {
        if ((((void *)gc_heaps[i].slot) <= ptr) &&
            ((size_t)ptr < (((size_t)gc_heaps[i].slot) + gc_heaps[i].size))) {
            return &gc_heaps[i];
        }
    }
    return NULL;
}
/**
 *
 * @param ptr
 * @param i
 * @return
 */
GC_Heap* is_pointer_to_space(void *ptr,size_t i)
{
    if ((((void *)gc_heaps[i].slot) <= ptr) &&
        ((size_t)ptr < (((size_t)gc_heaps[i].slot) + gc_heaps[i].size))) {
        return &gc_heaps[i];
    }
    return NULL;
}
/**
 * 获取该指针的header头 并再次检查是否是指针
 * @param gh
 * @param ptr
 * @return
 */
Header*  get_header(GC_Heap *gh, void *ptr)
{
    Header *p, *pend, *pnext;

    pend = (Header *)((size_t)gh->slot + gh->size);
    for (p = gh->slot; p < pend; p = pnext) {
        pnext = NEXT_HEADER(p);
        if ((void *)(p+1) <= ptr && ptr < (void *)pnext) {
            return p;
        }
    }
    return NULL;
}