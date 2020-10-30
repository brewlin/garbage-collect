#include "gc.h"
int clear(){
    free_list = NULL;
    sweeping = 0;
    for (int i = 0; i <= gc_heaps_used; ++i){
        gc_heaps[i].size = 0;
        gc_heaps[i].slot = NULL;
    }
    gc_heaps_used = 0;

    for (int j = 0; j <= root_used ; ++j){
        roots[j] = NULL;
    }
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
    gc_init(sizeof(Obj) + HEADER_SIZE, 1);
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
    gc_init(sizeof(Obj) + HEADER_SIZE, 3);
    //一次扫描3个堆
    max_sweep = 3;
    //一次标记1个对象
    max_mark  = 1;

    Obj *p1 = gc_malloc(sizeof(Obj));
    add_roots(p1);
    assert(p1);
    Obj *p2 = gc_malloc(sizeof(Obj));
    add_roots(p2);
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
 * 对引用进行测试
 */
void test_reference_gc()
{
    printf("-----------测试引用gc         ------------\n");
    printf("-----------***************------------\n");


    Obj* p   = gc_malloc(sizeof(Obj));
    p->v     = 10;
    p->left  = gc_malloc(sizeof(Obj));
    p->right = gc_malloc(sizeof(Obj));
    p->left->v = 11;
    p->right->v = 12;

    //加入root 即使left right 没有加入 但是他们作为 p的子节点引用 会被标记
    add_roots(p);
    gc();
    assert(p->v == 10);
    assert(p->left->v == 11);
    assert(p->right->v == 12);

    p   = gc_malloc(sizeof(Obj));
    p->v     = 10;
    p->left  = gc_malloc(sizeof(Obj));
    p->left->v = 11;
    Obj* left = p->left;
    //没有加入root 会被清除
    gc();
    assert(p->v == 0);
    assert(p->left == NULL);
    //子节点也会被清除
    assert(left->v == 0 );


    printf("-----------   passing     ------------\n");
    clear();
}
/**
 * 测试的时候需要关闭 自动gc
 */
void test_malloc_speed(){
    auto_gc = 0;
    time_t start,end;
    start = time(NULL);
//    for (int i = 0; i < 1000; ++i) {
//        int size = rand()%90;
//        void *p = gc_malloc(size);
//    }
    for (int i = 0; i < 10000; ++i) {
        int size = rand()%90;
        void *p = gc_malloc(size);
    }
    void *p = gc_malloc(24);
    p = gc_malloc(24);
    p = gc_malloc(24);
    gc();
    end = time(NULL);
    printf("execution seconds:%f\n",difftime(end,start));
}
int  main(int argc, char **argv)
{

    //小内存测试，
    test_malloc_free();
    clear();
    //测试gc
    test_gc(8);
    clear();
    //对象引用测试
    test_reference_gc();
    clear();

    //TODO: gc problem
//    test_malloc_speed();
    return 0;
}