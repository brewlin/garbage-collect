#include "gc.h"
#include "stack.h"
#include "tri-color.h"

Stack stack;
int gc_phase  = GC_ROOT_SCAN;
int max_mark  = 0;
int max_sweep = 0;
int sweeping  = 0;


void gc_init(size_t heap_size)
{
    for (size_t i = 0; i < gc_heaps_used; i++){
        //使用sbrk 向操作系统申请大内存块
        void* p = sbrk(heap_size + PTRSIZE + HEADER_SIZE);
        if(p == NULL)exit(-1);

        gc_heaps[i].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
        gc_heaps[i].size = heap_size;
        gc_heaps[i].slot->size = heap_size;
        gc_heaps[i].slot->next_free = NULL;

        Header* up = gc_heaps[i].slot;

        if(free_list == NULL){
            memset(up +  1,0,up->size);
            free_list = up;
            up->flags = 0;
        }else{
            gc_free((void *)(up+1));
        }
    }

}
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
    if (!(gh = is_pointer_to_heap(ptr))){
//      printf("not pointer\n");
      return;
    } 
    if (!(hdr = get_header(gh, ptr))) {
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
    //进行子节点 递归
    for (void* p = ptr; p < (void*)NEXT_HEADER(hdr); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        gc_mark(*(void **)p);
    }

}

/**
 * 为了防止在中断gc后 其他操作导致之前gc的状态发生异常
 * 需要引入写入屏障，在更新的时候需要调用处理
 * @param obj_ptr
 * @param field
 * @param new_obj_ptr
 */
void write_barrier(void *obj_ptr,void *field,void* new_obj_ptr)
{

    Header* obj     = CURRENT_HEADER(obj_ptr);
    Header* new_obj = CURRENT_HEADER(new_obj_ptr);
    //如果老对象已经被标记了 就要检查新对象是否标记了
    if(IS_MARKED(obj)){
        if(!IS_MARKED(new_obj)){
            FL_SET(new_obj,FL_MARK);
            push(&stack,new_obj_ptr);
        }
    }
    //obj->field = new_obj
    *(void **)field = new_obj_ptr;

}
/**
 * root 扫描阶段 将所有的root可达对象加入队列中
 */
void root_scan_phase()
{
    //垃圾回收前 先从 root 开始 进行递归标记
    for(int i = 0;i < root_used;i++)
    {
        void* ptr = roots[i].ptr;
        GC_Heap *gh;
        Header *hdr;
        if (!(gh = is_pointer_to_heap(ptr))) continue;
        if (!(hdr = get_header(gh, ptr)))    continue;
        if (!FL_TEST(hdr, FL_ALLOC))         continue;
        if (FL_TEST(hdr, FL_MARK))           continue;

        //标记为灰色 并入栈
        FL_SET(hdr, FL_MARK);
        push(&stack,ptr);
    }
    gc_phase = GC_MARK;

}
/**
 * 标记阶段
 */
void mark_phase()
{
    //1 全部将灰色标记完了在进行下一个清除阶段
    //2 未全部标记完则继续进行标记

    int scan_root = 0;
    for (int i = 0; i < max_mark; ++i) {
        //如果为空就继续去扫描一下root  看看在gc休息期间是否有新的没有进行标记
        if(empty(&stack)){
            //如果扫描过了root，但是依然没有新增灰色对象 则结束标记
            if(scan_root >= 1) {
                gc_phase = GC_SWEEP;
                break;
            }
            root_scan_phase();
            scan_root++;
            continue;
        }
        void* obj = pop(&stack);
        Header* hdr = CURRENT_HEADER(obj);
        //递归对child 进行标记
        for (void* p = obj; p < (void*)NEXT_HEADER(hdr); p++) {
            //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
            gc_mark(*(void **)p);
        }
    }
    //所有gc扫描完以后 只有空栈的话 说明标记完毕 需要进行清扫
    if(empty(&stack)){
        gc_phase = GC_SWEEP;
    }

}
/**
 * 清除 未标记内存 进行回收利用
 */
void sweep_phase(void)
{
    size_t i;
    Header *p, *pend, *pnext;

    //遍历所有的堆内存
    //因为所有的内存都从堆里申请，所以需要遍历堆找出待回收的内存
    for (i = sweeping; i < gc_heaps_used && i < max_sweep + sweeping; i++) {
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

    //如果堆扫描完则 切换到root扫描
    sweeping = i;
    if(i == gc_heaps_used){
        sweeping = 0;
        gc_phase = GC_ROOT_SCAN;
    }
}
void  gc(void)
{
    printf("执行gc\n");
    switch(gc_phase){
        case GC_ROOT_SCAN:
            root_scan_phase();
            return;
        case GC_MARK:
            mark_phase();
            return;
        case GC_SWEEP:
            sweep_phase();
    }
}

