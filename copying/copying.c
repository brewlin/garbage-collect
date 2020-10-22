#include "gc.h"


//保存了所有申请的对象
root roots[ROOT_RANGES_LIMIT];
size_t root_used = 0;

/**
 * 对该对象进行标记
 * 并进行子对象标记
 * @param ptr
 */
void gc_copy(void * ptr)
{
    Header *hdr;

    if (!(hdr = get_header(gh, ptr))) {
      printf("not find header\n");
      return;
    }
    //没有复制过  0
    if(!hdr->flags){
        //计算复制后的指针
        void *forwarding = free;
        //赋值
        memcpy(forwarding, hdr, hdr->size+HEADER_SIZE);
        free += hdr->size+HEADER_SIZE;
        obj->forwarding = forwarding;
        gc_copy_range((void *)(hdr+1), (void *)NEXT_HEADER(hdr));
        return forwarding;
    }
    //直接返回防止多次拷贝
    return hdr->forwarding;
}
/**
 * 遍历root 进行标记
 * @param start
 * @param end
 */
Header*  gc_copy_range(void *start, void *end)
{
    void *p;

    //start 表示当前对象 需要释放
    void* new_obj = gc_copy(start);
    //可能申请的内存 里面又包含了其他内存
    for (p = start+1; p < end; p++) {
         //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
         gc_copy(*(void **)p);
    }
    return (Header*)new_obj - 1;
}
/**
 * 将分配的变量 添加到root 引用,只要是root上的对象都能够进行标记
 * @param start
 * @param end
 */
void     add_roots(void * start, void * end)
{
    roots[root_used].start = start;
    roots[root_used].end = end;
    root_used++;

    if (root_used >= ROOT_RANGES_LIMIT) {
        fputs("Root OverFlow", stderr);
        abort();
    }
}
void  gc(void)
{
    //每次gc前jiang free指向 to的开头
    free = to + 1;
    //递归进行复制  从 from  => to
    for(int i = 0;i < root_used;i++){
        Header* forwarded = gc_copy_range(roots[i].start, roots[i].end);
        roots[i].start = forwarded;
        roots[i].end   = (void*)(forwarded+1) + forwarded->size;
    }
    //开始拷贝
    Header* tmp = from;
    from = to;
    to = tmp;
}

