#include "gc.h"
/**
 * 初始化所有的堆
 **/
void gc_init(size_t heap_size)
{
    //关闭自动扩充堆
    auto_grow = 0;

    gc_heaps_used = 1;
    //使用sbrk 向操作系统申请大内存块
    void* p = sbrk(heap_size + PTRSIZE);
    gc_heaps[0].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
    gc_heaps[0].size = heap_size;
    gc_heaps[0].slot->size = heap_size;
    gc_heaps[0].slot->next_free = NULL;
    //将堆初始化到free_list 链表上
    gc_free(gc_heaps[0].slot + 1);
}
/**
 * 开始进行gc标记
 **/
void* gc_mark(void *ptr){
    GC_Heap *gh;
    Header *hdr;

    /* mark check */
    if (!(gh = is_pointer_to_heap(ptr))){
//      printf("not pointer\n");
        return ptr;
    }
    if (!(hdr = get_header(gh,ptr))) {
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
    for (void* p = ptr; p < (void*)NEXT_HEADER(hdr); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        //递归进行 引用的拷贝
        gc_mark(*(void **)p);
    }
    return ptr;
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
                    new_obj = (void*)new_obj + p->size;
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
        Header* forwarding =  CURRENT_HEADER(roots[i].ptr)->forwarding;
        roots[i].ptr = forwarding+1;
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
                //正确找到了 child 引用
                GC_Heap *gh;
                Header  *hdr;
                if (!(gh = is_pointer_to_heap(*(void**)obj))) continue;
                if((hdr = get_header(gh,*(void**)obj))) {
                    *(Header **) obj = hdr->forwarding + 1; //更新引用
                }
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

        //TODO: 这里只有一个堆，所以空闲链表直接从内存开始处算起
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
                    memcpy(new_obj, p, p->size);
                    FL_UNSET(new_obj,FL_MARK);
                    //空闲链表下移
                    free_p = (void*)free_p + new_obj->size;
                    total -=  new_obj->size;

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
/**
 * 执行gc
 */
void  gc(void)
{
    printf("start gc()\n");
    //gc 递归标记
    for(int i = 0;i < root_used;i++)
        gc_mark(roots[i].ptr);

    //设置forwarding指针
    set_forwarding_ptr();
    //调整指针
    adjust_ptr();
    //移动对象
    move_obj();
}

