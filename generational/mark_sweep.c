/**
 * 专用于老年代gc的标记清除算法
 */

#include "gc.h"
#include "generational.h"
/**
 * 查看老年代header
 * @param ptr
 * @return
 */
Header*  get_old_header(void *ptr)
{
    GC_Heap* gh;

    if (((void*)gc_heaps[oldg].slot <= ptr) && ptr < gc_heaps[oldg].end)
        gh =  &gc_heaps[oldg];
    else
        return NULL;

    Header* hp = gh->slot;
    if ((void*)hp > ptr && gh->end <= ptr)
        return NULL;

    Header *p, *pend, *pnext;

    for (p = hp; (void*)p < gh->end; p = pnext) {
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
 * @param ptr
 */
void gc_mark(void * ptr)
{
    Header *hdr;
    //老年代gc 只需要检查是否是老年代区即可
    if (!(hdr = get_old_header(ptr))) {
      printf("not find header\n");
      return;
    }
    if (!FL_TEST(hdr, FL_ALLOC)) {
      printf("flag not set alloc\n");
      return;
    }
    if (FL_TEST(hdr, FL_MARK)) {
      //printf("flag not set mark\n");
      return;
    }

    /* marking */
    FL_SET(hdr, FL_MARK);
//    printf("mark ptr : %p, header : %p\n", ptr, hdr);
    //进行子节点递归 标记
    for (void* p = ptr + 1; p < (void*)NEXT_HEADER(hdr); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        gc_mark(*(void **)p);
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

    //老年代gc 只清除 老年代堆即可
    for (p = gc_heaps[oldg].slot; (void*)p < gc_heaps[oldg].end; p = NEXT_HEADER(p)) {
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
/**
 * 老年代 gc
 */
void  major_gc(void)
{
    //rs 里的基本都是老年代
    for(int i = 0; i < rs_index; i ++) {
        //只对老年代 对象进行gc
        gc_mark(rs[i]);
    }
    //标记完成后 在进行 清除 对于没有标记过的进行回收
    gc_sweep();
}

