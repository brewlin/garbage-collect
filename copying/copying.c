#include "gc.h"


//每次复制前将 该指针 指向 to的首地址
void* free_p;
GC_Heap from;
GC_Heap to;

/**
 * 增加堆
 **/
void gc_init(size_t req_size)
{
    auto_gc   = 1;
    //关闭自动扩充堆
    auto_grow = 0;
    //使用sbrk 向操作系统申请大内存块
    void* from_p = sbrk(req_size + PTRSIZE );
    from.slot  = (Header *)ALIGN((size_t)from_p, PTRSIZE);
    from.slot->next_free = NULL;
    from.slot->size = req_size;
    from.size       = req_size;
    gc_free((void*)(from.slot + 1));
    DEBUG(printf("扩堆内存:%ld ptr:%p\n",req_size,from_p));

    //使用sbrk 向操作系统申请大内存块
    void* to_p = sbrk(req_size + PTRSIZE + HEADER_SIZE);
    to.slot  = (Header *)ALIGN((size_t)to_p, PTRSIZE);
    to.slot->next_free = NULL;
    to.slot->size = req_size;
    to.size = req_size;
}
//重from堆 检查该指针
Header*  get_header_by_from(void *ptr)
{
    size_t i;
    Header* from_ptr = from.slot;
    if ((((void *)from_ptr) > ptr) && ((((void*)from_ptr) +  from.size)) <= ptr) {
        return NULL;
    }
    Header *p, *pend, *pnext;

    pend = (Header *)(((void*)(from_ptr)) + from.size);
    for (p = from_ptr; p < pend; p = pnext) {
        pnext = NEXT_HEADER(p);
        if ((void *)(p+1) <= ptr && ptr < (void *)pnext) {
            return p;
        }
    }
    return NULL;
}
/**
 * 对该对象进行标记
 * 并进行子对象标记
 * 返回to空间的 body
 * @param ptr
 */
void* gc_copy(void * ptr)
{
    Header *hdr;

    if (!(hdr = get_header_by_from(ptr))) return NULL;
    //没有复制过  0
    if(!IS_COPIED(hdr)){
        //计算复制后的指针
        Header *forwarding = (Header*)free_p;
        //在准备分配前的总空间
        size_t total = forwarding->size;
        //分配一份内存 将源对象拷贝过来
        memcpy(forwarding, hdr, hdr->size);
        //标记为已拷贝
        FL_SET(hdr,FL_COPIED);
        hdr->flags = 1;
        //free 指向下一个 body
        free_p += hdr->size;
        //free_p 执行的剩余空间需要时刻维护着
        ((Header*)free_p)->size = total - hdr->size;
        //源对象 保留 新对象的引用
        hdr->forwarding = forwarding;

        printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);

        //从forwarding 指向的空间开始递归
        for (void* p = (void*)(forwarding+1); p < (void*)NEXT_HEADER(forwarding); p++) {
            //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
            //递归进行 引用的拷贝
            gc_copy(*(void **)p);
        }
        //返回body
        return forwarding + 1;
    }
    //forwarding 是带有header头部的，返回body即可
    return hdr->forwarding+1;
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
        for (void *p = start; p < end;  p++) {

            Header* hdr;
            //解引用 如果该内存依然是指向的from，且有forwarding 则需要改了
            void *ptr = *(void**)p;
            if (!(hdr = get_header_by_from(ptr))) {
                continue;
            }
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
    //每次gc前jiang free指向 to的开头
    to.slot->size = to.size;
    free_p = to.slot;

    //递归进行复制  从 from  => to
    for(int i = 0;i < root_used;i++){
        void* forwarded = gc_copy(roots[i].ptr);
        *(Header**)roots[i].optr = forwarded;
        //将root 所有的执行换到 to上
        roots[i].ptr = forwarded;
    }
    copy_reference();

    //清空 from
    memset(from.slot,0,from.size+HEADER_SIZE);

    //开始交换from 和to
    Header* tmp = from.slot;
    from.slot = to.slot;
    to.slot = tmp;
    //将空闲链表放到 to的最后一个索引
    free_list = free_p;
}

