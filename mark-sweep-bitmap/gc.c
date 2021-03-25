#include "gc.h"
#include <time.h>

//回收链表，挂着空闲链表
Header* free_list = NULL;
GC_Heap heap;
root    roots[ROOT_RANGES_LIMIT]; //根
size_t  root_used = 0;

/**
 * 为了更好的描述位图标记，目前只初始化话一个堆
 * 且不允许扩充堆
 */
/**
 * 增加堆
 **/
void gc_init()
{
    void *p;
    if((p = sbrk(TINY_HEAP_SIZE)) == (void *)-1){
        DEBUG(printf("sbrk 分配内存失败\n"));
        return NULL;
    }
    heap.slot = p;
    heap.size = TINY_HEAP_SIZE;
    heap.end  = p + TINY_HEAP_SIZE;
    memset(p,0,heap.size);
    //这里要将位图和内存区区分开来
    // 1bit 对应8字节对象，是64倍关系
    int bit = ALIGN(TINY_HEAP_SIZE/(PTRSIZE*8 + 1),64);
    heap.bit_size = bit;
    int obj_size =  TINY_HEAP_SIZE - bit;
    heap.bit  = heap.slot + bit;

    //heap.bit 之后的位置才是内存分配区
    Header* up = (Header *) heap.bit;
    up->size   = obj_size;
    //初始化空闲链表
    free_list = up;
}
/**
 * 将分配的变量 添加到root 引用,只要是root上的对象都能够进行标记
 * @param start
 * @param end
 */
void add_roots(void* ptr)
{
    roots[root_used].ptr = ptr;
    root_used++;
    if (root_used >= ROOT_RANGES_LIMIT) {
        fputs("Root OverFlow", stderr);
        abort();
    }
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

            //新的内存 是包括了 header + mem 所以返回给 用户mem部分就可以了
            return (void *)(p+1);
        }

    }

    //这里表示前面多次都没有找到合适的空间，且已经遍历完了空闲链表 free_list
    //这里表示在 单次内存申请的时候 且 空间不够用的情况下 需要执行一次gc
    //一般是分块用尽会 才会执行gc 清除带回收的内存
    if (!do_gc) {
        gc();
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
    Header *target, *hit,*prevp;
    //通过内存地址向上偏移量找到  header头
    target = (Header *)ptr - 1;
    //回收的数据立马清空
//    memset(ptr,0,target->size-HEADER_SIZE);

    //空闲链表为空，直接将当前target挂到上面
    if(free_list == NULL){
        free_list = target;
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
