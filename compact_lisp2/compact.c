#include "gc.h"
#include "compact.h"

//保存了所有申请的对象
root roots[ROOT_RANGES_LIMIT];
size_t root_used = 0;
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
    gc_mark_range((void *)(hdr+1) + 1, (void *)NEXT_HEADER(hdr));
    return ptr;
}

/**
 * 遍历root 进行标记
 * @param start
 * @param end
 */
void  gc_mark_range(void *start, void *end)
{
    void *p;

    gc_mark(start);
    //可能申请的内存 里面又包含了其他内存
    for (p = start; p < end; p++) {
         //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
         //递归进行 引用的拷贝
         gc_mark(*(void **)p);
    }
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

void     set_forwarding_ptr(void)
{
    size_t i;
    Header *p, *pend, *pnext ,*new_obj;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (i = 0; i < gc_heaps_used; i++) {
        //pend 堆内存结束为止
        pend = (Header *)(((size_t)gc_heaps[i].slot) + gc_heaps[i].size);
        p = gc_heaps[i].slot;
        new_obj = gc_heaps[i].slot;
        //堆的起始为止 因为堆的内存可能被分成了很多份，所以需要遍历该堆的内存
        for (; p < pend; p = NEXT_HEADER(p))
        {
            //查看该堆是否已经被使用
            if (FL_TEST(p, FL_ALLOC)) {
                //查看该堆是否被标记过
                if (FL_TEST(p, FL_MARK)) {
                    p->forwarding = new_obj;
                    //new_obj 继续下移 p个空间大小
                    new_obj = (void*)(new_obj+1) + p->size;
                }
            }
        }
    }
}
/**
 * 遍历root
 * 遍历堆
 */
void adjust_ptr()
{
    //遍历所有对象
    for(int i = 0; i < root_used; i ++){
        Header* forwarding =  CURRENT_HEADER(roots[i].start)->forwarding;
        roots[i].start = forwarding+1;
        roots[i].end   = (void*)(forwarding+1) + forwarding->size;
        *(Header**)roots[i].optr = forwarding+1;
    }
    //下面更新引用
    size_t i;
    Header *p, *pend, *pnext ,*new_obj;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (i = 0; i < gc_heaps_used; i++)
    {
        //pend 堆内存结束为止
        pend = (Header *)(((size_t)gc_heaps[i].slot) + gc_heaps[i].size);

        //堆的起始为止 因为堆的内存可能被分成了很多份，所以需要遍历该堆的内存
        for (p = gc_heaps[i].slot; p < pend; p = NEXT_HEADER(p))
        {
            //可能申请的内存 里面又包含了其他内存
            for (void* obj = p+1; obj < (void*)NEXT_HEADER(p); obj++)
            {
                //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
                //递归进行 引用的拷贝
                Header* hdr = get_header(*(void**)obj);
                if(hdr)
                    //更新引用
                    *(Header**)obj = hdr->forwarding + 1;
            }
        }
    }

}
/**
 * 移动对象
 */
void move_obj()
{
    size_t total;
    Header *p, *pend, *pnext ,*new_obj,*free_p;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (size_t i = 0; i < gc_heaps_used; i++) {
        //pend 堆内存结束为止
        pend = (Header *)(((size_t)gc_heaps[i].slot) + gc_heaps[i].size);

        //TODO: 这里只有一个堆，所以空闲链表直接从内存循环算起了
        free_p = gc_heaps[i].slot;
        total = gc_heaps[i].size;
        //堆的起始为止 因为堆的内存可能被分成了很多份，所以需要遍历该堆的内存
        for (p = gc_heaps[i].slot; p < pend; p = NEXT_HEADER(p))
        {
            //查看该堆是否已经被使用
            if (FL_TEST(p, FL_ALLOC)) {
                //查看该堆是否被标记过
                if (FL_TEST(p, FL_MARK)) {
                    new_obj = p->forwarding;
                    memcpy(new_obj, p, p->size + HEADER_SIZE);
                    FL_UNSET(new_obj,FL_MARK);
                    //空闲链表下移
                    free_p = (void*)(free_p) + HEADER_SIZE + new_obj->size;
                    total -= HEADER_SIZE + new_obj->size;

                }
            }
        }
    }
    //这个时候free_p 后面就是空闲列表了
    free_list = free_p;
    //total 需要单独计算剩余空间
    free_list->size = total;
    free_list->next_free = NULL;
    //方便测试 把空闲空间都清空
    memset(free_list + 1,0,total);

}
void  gc(void)
{
    printf("start gc()\n");
    //gc 递归标记
    for(int i = 0;i < root_used;i++)
        gc_mark_range(roots[i].start, roots[i].end);

    //设置forwarding指针
    set_forwarding_ptr();
    //调整指针
    adjust_ptr();
    //移动对象
    move_obj();
}

