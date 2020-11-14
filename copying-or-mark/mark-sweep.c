#include "gc.h"
#include "header.h"

/**
 * 开始进行gc标记
 **/
void* gc_mark(void *ptr){
    GC_Heap *gh;
    Header *hdr;
    //因为外面（mark_or_copy）已经判断过了肯定是来自非 from to 堆
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
    for (void* p = (void*)(hdr + 1); p < (void*)NEXT_HEADER(hdr); p++) {
        //对子类依然需要进行检查是否来自于from,to堆,否则不需要进行标记
        gc_mark_or_copy(*(void **)p);
    }
    return ptr;
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
                    gc_free(p+1);
                }
            }
        }
    }
}