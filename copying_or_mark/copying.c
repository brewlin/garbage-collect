#include "gc.h"
#include "copying.h"

//保存了所有申请的对象
root roots[ROOT_RANGES_LIMIT];
size_t root_used = 0;
//每次复制前将 该指针 指向 to的首地址
void* free_p;
/**
 * 查看该对象是否存在于from堆上
 */
int is_pointer_to_from_space(void* ptr)
{

    void* from_start = gc_heaps[from].slot;
    void* from_end   = (void*)gc_heaps[from].slot + HEADER_SIZE + gc_heaps[from].size;
    if((void*)ptr <= from_end && (void*)ptr >= from_start)
        return 1;
    return 0;
}

/**
 * 开始进行gc标记
 **/
void* gc_mark(void *ptr){
    Header *hdr;
    if (!(hdr = get_header(ptr))) {
//      printf("not find header\n");
      return ptr;
    }
    if (!FL_TEST(hdr, FL_ALLOC)) {
      printf("flag not set alloc\n");
      return ptr;
    }
    if (FL_TEST(hdr, FL_MARK)) {
      //printf("flag not set mark\n");
      return ptr;
    }

    /* marking */
    FL_SET(hdr, FL_MARK);
//    printf("mark ptr : %p, header : %p\n", ptr, hdr);
    //进行子节点递归 标记
    gc_mark_or_copy_range((void *)(hdr+1) + 1, (void *)NEXT_HEADER(hdr));
    return ptr;
}
/**
 * 进行子对象拷贝
 */
void* gc_copy(void *ptr){
    Header *hdr;

    if (!(hdr = get_header(ptr))) {
//      printf("not find header\n");
      return NULL;
    }
    assert(FL_TEST(hdr,FL_ALLOC));
    //没有复制过  0 
    if(!IS_COPIED(hdr)){
        //计算复制后的指针
        Header *forwarding = (Header*)free_p;
        //在准备分配前的总空间
        size_t total = forwarding->size;
        //分配一份内存 将源对象拷贝过来
        memcpy(forwarding, hdr, hdr->size+HEADER_SIZE);
        //拷贝过后 将原对象标记为已拷贝 为后面递归引用复制做准备
        FL_SET(hdr,FL_COPIED);
        //free 指向下一个 body
        free_p += (HEADER_SIZE + hdr->size);
        //free_p 执行的剩余空间需要时刻维护着
        ((Header*)free_p)->size = total - (hdr->size + HEADER_SIZE);
        //源对象 保留 新对象的引用
        hdr->forwarding = forwarding;

        printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);
        //从forwarding 指向的空间开始递归
        gc_mark_or_copy_range((void *)(forwarding+1) + 1, (void *)NEXT_HEADER(forwarding));
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
void* gc_mark_or_copy(void * ptr)
{
    if(is_pointer_to_from_space(ptr))
        return gc_copy(ptr);   
    return gc_mark(ptr);
}
/**
 * 遍历root 进行标记
 * @param start
 * @param end
 */
void*  gc_mark_or_copy_range(void *start, void *end)
{
    void *p;

    void* new_obj = gc_mark_or_copy(start);
    //可能申请的内存 里面又包含了其他内存
    for (p = start + 1; p < end; p++) {
         //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
         //递归进行 引用的拷贝
         gc_mark_or_copy(*(void **)p);
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
/**
 * 清除 未标记内存 进行回收利用
 */
void     gc_sweep(void)
{
    size_t i;
    Header *p, *pend, *pnext;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (i = 0; i < gc_heaps_used; i++) {
        //to 和 from堆不需要进行清除
        if(i == from || i == to) continue;
        //pend 堆内存结束为止
        pend = (Header *)(((size_t)gc_heaps[i].slot) + gc_heaps[i].size);
        //堆的起始为止 因为堆的内存可能被分成了很多份，所以需要遍历该堆的内存
        for (p = gc_heaps[i].slot; p < pend; p = NEXT_HEADER(p)) {
            //查看该堆是否已经被使用
            if (FL_TEST(p, FL_ALLOC)) {
                //查看该堆是否被标记过
                if (FL_TEST(p, FL_MARK)) {
                    DEBUG(printf("解除标记 : %p\n", p));
                    //取消标记，等待下次来回收，如果在下次回收前
                    //1. 下次回收前发现该内存又被重新访问了，则不需要清除
                    //2. 下次回收前发现该内存没有被访问过，所以需要清除
                    FL_UNSET(p, FL_MARK);
                }else {
                    DEBUG(printf("清除回收 :\n"));
                    //回收的时候 要把header->size加上
                    p->size += HEADER_SIZE;
                    gc_free(p+1);
                }
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
        void* forwarded = gc_mark_or_copy_range(roots[i].start, roots[i].end);

        *(Header**)roots[i].optr = forwarded;
        //将root 所有的执行换到 to上
        roots[i].start = forwarded;
        roots[i].end   = forwarded + CURRENT_HEADER(forwarded)->size;
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

