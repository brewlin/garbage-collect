#include "gc.h"

/**
 * 压缩算法中方便测试算法本身，只允许一个堆且不允许自动个扩充
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
    //进行子节点递归 标记
    for (void* p = ptr; p < (void*)NEXT_HEADER(hdr); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        //递归进行 引用的拷贝
        gc_mark(*(void **)p);
    }
    return ptr;
}

/**
 * 移动对象
 */
void move_obj()
{
    //下面更新引用
    size_t i,total;
    Header *live;

    //只有一个堆
    for (i = 0; i < gc_heaps_used; i++) 
    {
        //右指针，从尾部开始遍历
        live = (Header *) ((void * )(gc_heaps[i].slot + 1) + gc_heaps[i].size);
        //左指针，从头部开始遍历
        free_list = gc_heaps[i].slot;
        total = gc_heaps[i].size;
        while (true) {
            //遍历到第一个非标记的地方，也就是空闲区
            while (FL_TEST(free_list, FL_ALLOC) && FL_TEST(free_list, FL_MARK) && free_list < live){
                FL_UNSET(free_list,FL_MARK);
                total -= free_list->size;
                free_list = NEXT_HEADER(free_list);
            }
            //遍历到第一个被标记了的地方，这样就会将这个地方拷贝到上面的空闲区
            while (!FL_TEST(live, FL_MARK) && live > gc_heaps[i].slot)
                //TODO:因为反向遍历的时候 没有域且内存非等分，所以不能通过 -= mem_size 来遍历
                live = (Header *) ((void *) live - 1);
            //进行拷贝
            if (free_list < live) 
            {
                FL_UNSET(live, FL_MARK);
                memcpy(free_list, live, live->size);
                live->forwarding = free_list;
                total -= live->size;
            } else {
                break;
            }
        }
    }
    free_list->size = total;
    free_list->next_free = NULL;
    //方便测试 把空闲空间都清空
    memset(free_list + 1,0,total);

}

/**
 * 遍历root
 * 遍历堆
 */
void adjust_ptr()
{
    //遍历所有对象 更新root
    for(int i = 0; i < root_used; i ++){
        Header* current    = CURRENT_HEADER(roots[i].ptr);
        Header* forwarding =  current->forwarding;
        //注意: 和lisp2不同的是，当前算法不会无差别移动所有对象，那么free_list右边会存在两种指针
        // 1. 发生了移动的对象
        // 2. 空闲对象
        //所以root指向的对象在free_list后面，说明发生了移动，需要更新root
        if(current >= free_list){
            roots[i].ptr = forwarding+1;
            *(Header**)roots[i].optr = forwarding+1;
        }
    }
    //下面更新引用
    size_t i;
    Header *p, *pend, *pnext ,*new_obj;

    //当前只有一个堆
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
void  gc(void)
{
    printf("start gc()\n");
    //gc 递归标记
    for(int i = 0;i < root_used;i++)
        gc_mark(roots[i].ptr);

    //移动对象
    move_obj();
    //调整指针
    adjust_ptr();
}

