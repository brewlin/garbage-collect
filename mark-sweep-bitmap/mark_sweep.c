#include "gc.h"


/**
 * 对该对象进行标记
 * 并进行子对象标记
 * @param ptr
 */
void gc_mark(void * ptr)
{
    GC_Heap *gh;
    Header *hdr;

    /* mark check */
    if (!IS_HEAP(ptr)){
//      printf("not pointer\n");
      return;
    }
    //进行标记
    int obj = (long int)((void*)(CURRENT_HEADER(ptr)) - (void*)heap.bit) / PTRSIZE;
    int index = obj / PTRSIZE;
    int offset = obj % PTRSIZE;
    //进行位标记
    void *bit = (void*)heap.slot + index;
    *(char*)bit |= 1 << offset;

    hdr = CURRENT_HEADER(ptr);
    //进行child 节点递归 标记
    for (void* p = ptr; p < (void*)NEXT_HEADER(hdr); p++) {
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
    Header *prev,*p, *pend, *pnext;
    int obj,index,offset;


    for (p = heap.bit; p < heap.end && prev != p; prev = p,p = NEXT_HEADER(p)) {
        //查看位图是否已经标记
        obj = (long int )((void*)p - (void*)heap.bit) / PTRSIZE;
        index = obj / PTRSIZE;
        offset = obj % PTRSIZE;

        void *bit = (void*)heap.slot + index;
        //查看该堆是否已经被使用 ，没有被标记的白色垃圾需要回收
        if ((*(unsigned char*)bit & (1 << offset)) == 0) {
//            DEBUG(printf("清除回收 :\n"));
            gc_free(p+1);
        }
    }
    //最后清空位图
    memset(heap.slot,0,heap.bit_size);
}
/**
 * 标记清除算法的gc实现
 */
void  gc(void)
{
    printf("gc start\n");
    //垃圾回收前 先从 root 开始 进行递归标记
    for(int i = 0;i < root_used;i++)
        gc_mark(roots[i].ptr);
    //标记完成后 在进行 清除 对于没有标记过的进行回收
    gc_sweep();
}

