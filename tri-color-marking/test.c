#include "gc.h"
#include "tri-color.h"
int clear(){
    free_list = NULL;
    sweeping = 0;
    gc_heaps_used = 0;
    root_used = 0;
    auto_gc = 1;
}
typedef struct obj{
    int v;
    struct obj* left;
    struct obj* right;
}Obj;
/**
 * 1. 测试条件保证申请内存保证在TINY_HEAP_SIZE下，因为大于这个数会发生gc
 * 测试 分配和 释放是否正常
 */
void test_malloc_free(){
    printf("-----------测试内存申请与释放------------\n");
    printf("-----------***************------------\n");

    //测试的时候关闭自动扩充堆
    //关闭自动gc
    auto_gc = 0;
    auto_grow = 0;
    //每个堆 可以存储1个Obj
    gc_heaps_used  = 1;
    gc_init(sizeof(Obj) + HEADER_SIZE);
    //在回收p1的情况下 p2的申请将复用p1的地址
    Obj *p1 = gc_malloc(sizeof(Obj));
    gc_free(p1);
    Obj *p2 = gc_malloc(sizeof(Obj));
    //在上面 p1被释放了，p2 从新申请 会继续从堆的起始位置开始分配 所以 内存地址是一样的
    assert(p1 == p2);

    Obj *p3 = gc_malloc(sizeof(Obj));
    assert(p3 == NULL);

    printf("-----------   passing     ------------\n\n");
}
void test_gc(){
    printf("-----------测试gc         ------------\n");
    printf("-----------***************------------\n");

    //测试的时候关闭自动扩充堆
    auto_grow = 0;
    //每个堆 可以存储1个Obj
    gc_heaps_used = 3;
    gc_init(sizeof(Obj) + HEADER_SIZE);
    //一次扫描3个堆
    max_sweep = 3;
    //一次标记1个对象
    max_mark  = 1;

    Obj *p1 = gc_malloc(sizeof(Obj));
    add_roots(&p1);
    assert(p1);
    Obj *p2 = gc_malloc(sizeof(Obj));
    add_roots(&p2);
    assert(p2);
    Obj *p3 = gc_malloc(sizeof(Obj));
    p3->v = 33;
    assert(p3);

    //空间不够 执行gc
    Obj *p4 = gc_malloc(sizeof(Obj));
    assert(p4 == NULL);
    assert(gc_phase == GC_MARK);

    //需要两次才能标记完
    p4 = gc_malloc(sizeof(Obj));
    assert(p4 == NULL);
    assert(gc_phase == GC_MARK);

    p4 = gc_malloc(sizeof(Obj));
    assert(p4 == NULL);
    assert(gc_phase == GC_SWEEP);


    //开始执行清除阶段 全部扫描完了
    //p3 被释放了
    p4 = gc_malloc(sizeof(Obj));
    assert(p3->v == 0);
    assert(p4);
    assert(gc_phase == GC_ROOT_SCAN);

    printf("-----------   passing     ------------\n\n");
}

/**
 * 测试写屏障
 */
void test_write_barrier()
{
    printf("-----------测试写屏障         ------------\n");
    printf("-----------***************------------\n");
    //测试的时候关闭自动扩充堆
    auto_grow = 0;
    auto_gc   = 1;
    //每个堆 可以存储1个Obj
    gc_heaps_used = 1;
    gc_init(4 * (sizeof(Obj) + HEADER_SIZE));
    //一次扫描3个堆
    max_sweep = 1;
    //一次标记1个对象
    max_mark  = 1;

    Obj* p1 = gc_malloc(sizeof(Obj));
    add_roots(&p1);
    //p1 被标记
    gc();
    assert(gc_phase == GC_MARK);

    gc();
    assert(gc_phase == GC_SWEEP);

    //接下来就是清除阶段了，但是我在这个阶段的时候 更新了原先的引用
    Obj* p2 = gc_malloc(sizeof(Obj));
    p2->v = 22;
    write_barrier(p1,&p1->left,p2);

    //清除完毕
    gc();
    assert(gc_phase == GC_ROOT_SCAN);
    assert(p1->left);
    assert(p2->v == 22);

    printf("-----------   passing     ------------\n");
    clear();
}
void test_write_barrier2()
{
    printf("-----------测试未加入root写屏障         ------------\n");
    printf("-----------***************------------\n");
    //测试的时候关闭自动扩充堆
    auto_grow = 0;
    auto_gc   = 1;
    //每个堆 可以存储4个Obj
    gc_heaps_used = 1;
    gc_init(4 * (sizeof(Obj) + HEADER_SIZE));
    //一次扫描3个堆
    max_sweep = 1;
    //一次标记1个对象
    max_mark  = 1;

    Obj* p1 = gc_malloc(sizeof(Obj));
    //p1 被标记
    gc();
    assert(gc_phase == GC_MARK);

    gc();
    assert(gc_phase == GC_SWEEP);

    //接下来就是清除阶段了，但是我在这个阶段的时候 更新了原先的引用
    Obj* p2 = gc_malloc(sizeof(Obj));
    p2->v = 22;
    write_barrier(p1,&p1->left,p2);

    //清除完毕
    gc();
    assert(gc_phase == GC_ROOT_SCAN);
    assert(p1->left == NULL);
    assert(p2->v == 0);

    printf("-----------   passing     ------------\n");
    clear();
}
int  main(int argc, char **argv)
{

    //小内存测试，
    test_malloc_free();
    clear();
    //测试gc
    test_gc();
    clear();
    //测试写屏障
    test_write_barrier();
    clear();
    //测试未加入root写屏障
    test_write_barrier2();
    clear();
    

    return 0;
}