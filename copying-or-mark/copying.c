#include "../gc.h"
#include "header.h"

//每次复制前将 该指针 指向 to的首地址
void* free_p;
int from = 1;
int to   = 0;

/**
 * 初始化所有的堆
 **/
void gc_init(size_t heap_size)
{
    //关闭扩充堆
    auto_grow = 0;
    //默认三个堆用于标记清除算法
    gc_heaps_used = 3;

    for (size_t i = 0; i < gc_heaps_used; i++){
        //使用sbrk 向操作系统申请大内存块
        void* p = sbrk(heap_size + PTRSIZE);
        gc_heaps[i].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
        gc_heaps[i].size = heap_size;
        gc_heaps[i].slot->size = heap_size;
        gc_heaps[i].slot->next_free = NULL;
        //默认情况下0 是给 to堆使用的  不需要挂载到 free_list 空闲链表上
        if(i) gc_free(gc_heaps[i].slot + 1);
    }
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
        {
            //由可能 free_list 表头就是来自于from堆
            if(free_list == prevp)
                free_list = p->next_free;
            else
                prevp->next_free = p->next_free;
        }
    }

}
/**
 * 进行子对象拷贝
 */
void* gc_copy(void *ptr)
{
    Header  *hdr;
    GC_Heap *gh;
    if (!(gh = is_pointer_to_space(ptr,from))) return NULL;
    if (!(hdr = get_header(gh,ptr))) return NULL;

    assert(FL_TEST(hdr,FL_ALLOC));
    //没有复制过  0 
    if(!IS_COPIED(hdr)){
        //计算复制后的指针
        Header *forwarding = (Header*)free_p;
        //在准备分配前的总空间
        size_t total = forwarding->size;
        //分配一份内存 将源对象拷贝过来
        memcpy(forwarding, hdr, hdr->size);
        //拷贝过后 将原对象标记为已拷贝 为后面递归引用复制做准备
        FL_SET(hdr,FL_COPIED);
        //free 指向下一个 body
        free_p += hdr->size;
        //free_p 执行的剩余空间需要时刻维护着
        ((Header*)free_p)->size = total - hdr->size;
        //源对象 保留 新对象的引用
        hdr->forwarding = forwarding;

        printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);

        //从forwarding 指向的空间开始递归
        for (void* p = (void*)(forwarding + 1); p < (void*)NEXT_HEADER(forwarding); p++) {
            //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
            //递归进行 引用的拷贝
            gc_mark_or_copy(*(void **)p);
        }
        //返回body
        return forwarding + 1;
    }
    //forwarding 是带有header头部的，返回body即可
    return hdr->forwarding+1;
}
/**
 * 对该对象进行标记 或拷贝
 * 并进行子对象标记 或拷贝
 * 返回to空间的 body
 * @param ptr
 */
void* gc_mark_or_copy(void* ptr)
{
    if(is_pointer_to_space(ptr,from))
        return gc_copy(ptr);   
    return gc_mark(ptr);
}
/**
 * 拷贝引用
 * 在将所有的from拷贝到to后 其实 对象的引用还是指向的from，所以需要遍历to对他们进行解引用
 */
void copy_reference()
{
    //遍历所有对象
    for(int i = 0; i < root_used; i ++)
    {
        void* start =  roots[i].ptr;
        void* end   =  (void*)NEXT_HEADER(CURRENT_HEADER(start));

        //可能申请的内存 里面又包含了其他内存
        for (void *p = start; p < end; p++) {

            Header  *hdr;
            GC_Heap *gh;
            if (!(gh = is_pointer_to_space(*(void**)p,from))) continue;
            //解引用 如果该内存依然是指向的from，且有forwarding 则需要改了
            if (!(hdr = get_header(gh,*(void**)p)))    continue;
            if(hdr->forwarding){
                printf("拷贝引用 hdr:%p forwarding:%p\n",hdr,hdr->forwarding);
                *(Header**)p = hdr->forwarding + 1;
                break;
            }

        }
    }
}


void  gc(void)
{
    printf("执行gc复制----\n");
    //每次gc前将 free指向 to的开头
    gc_heaps[to].slot->size = gc_heaps[to].size;
    free_p = gc_heaps[to].slot;

    //递归进行复制  从 from  => to
    for(int i = 0;i < root_used;i++){
        void* forwarded = gc_mark_or_copy(roots[i].ptr);
        *(Header**)roots[i].optr = forwarded;
        //将root 所有的执行换到 to上
        roots[i].ptr = forwarded;
    }
    copy_reference();
    //其他部分执行gc清除
    gc_sweep();
    //首先将free_p 指向的剩余空间  挂载到空闲链表上 
    //其实就是将原先to剩余的空间继续利用起来

    //如果没有剩余空间了则不进行操作
    if(free_p < ((void*)gc_heaps[to].slot + gc_heaps[to].size))
        gc_free((Header*)free_p+1);
    //在gc的时候 from已经全部复制到to堆
    //这个时候需要清空from堆，但是在此之前我们需要将free_list空闲指针还保留在from堆上的去除
    remove_from();
    /**
     * 清空 from 空间前:
     * 因为空闲链表 还指着from空间的，所以需要更新free_list 指针
     * 
     */
    memset(gc_heaps[from].slot,0,gc_heaps[from].size+HEADER_SIZE);

    //开始交换from 和to
    to = from;
    from = (from + 1)%10;
}

