#include "gc.h"
#include "copying.h"

//保存了所有申请的对象
root roots[ROOT_RANGES_LIMIT];
size_t root_used = 0;
//每次复制前将 该指针 指向 to的首地址
void* free_p;

/**
 * 对该对象进行标记
 * 并进行子对象标记
 * 返回to空间的 body
 * @param ptr
 */
void* gc_copy(void * ptr)
{
    Header *hdr;

    if (!(hdr = get_header(ptr))) {
//      printf("not find header\n");
      return NULL;
    }
    //没有复制过  0
    if(!hdr->flags){
        //计算复制后的指针
        Header *forwarding = (Header*)free_p;
        //在准备分配前的总空间
        size_t total = forwarding->size;
        //分配一份内存 将源对象拷贝过来
        memcpy(forwarding, hdr, hdr->size+HEADER_SIZE);
        //标记为已拷贝
        hdr->flags = 1;
        //free 指向下一个 body
        free_p += (HEADER_SIZE + hdr->size);
        //free_p 执行的剩余空间需要时刻维护着
        ((Header*)free_p)->size = total - (hdr->size + HEADER_SIZE);
        //源对象 保留 新对象的引用
        hdr->forwarding = forwarding;

        printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);
        //从forwarding 指向的空间开始递归
        gc_copy_range((void *)(forwarding+1) + 1, (void *)NEXT_HEADER(forwarding));
        //返回body
        return forwarding + 1;
    }
    //forwarding 是带有header头部的，返回body即可
    return hdr->forwarding+1;
}
/**
 * 遍历root 进行标记
 * @param start
 * @param end
 */
void*  gc_copy_range(void *start, void *end)
{
    void *p;

    void* new_obj = gc_copy(start);
    //可能申请的内存 里面又包含了其他内存
    for (p = start + 1; p < end; p++) {
         //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
         //递归进行 引用的拷贝
         gc_copy(*(void **)p);
    }
    //返回 body
    return new_obj;
}
/**
 * 将分配的变量 添加到root 引用,只要是root上的对象都能够进行标记
 * @param start
 * @param end
 */
void     add_roots(void* o_ptr)
{
    void *ptr = *(void**)o_ptr;

    Header* hdr = (Header*)ptr - 1;
    roots[root_used].start = ptr;
    roots[root_used].end = ptr + hdr->size;
    roots[root_used].optr = o_ptr;
    root_used++;

    if (root_used >= ROOT_RANGES_LIMIT) {
        fputs("Root OverFlow", stderr);
        abort();
    }
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
        void* start =  roots[i].start;
        void* end   =  roots[i].end;

        //可能申请的内存 里面又包含了其他内存
        for (void *p = start; p < end; p++) {

            Header* hdr;
            //解引用 如果该内存依然是指向的from，且有forwarding 则需要改了
            void *ptr = *(void**)p;
            if (!(hdr = get_header(ptr))) {
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
        void* forwarded = gc_copy_range(roots[i].start, roots[i].end);

        *(Header**)roots[i].optr = forwarded;
        //将root 所有的执行换到 to上
        roots[i].start = forwarded;
        roots[i].end   = forwarded + CURRENT_HEADER(forwarded)->size;
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

