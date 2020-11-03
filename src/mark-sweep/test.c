#include "gc.h"
int clear(){
    free_list = NULL;
    for (int i = 0; i <= gc_heaps_used; ++i){
        gc_heaps[i].size = 0;
        gc_heaps[i].slot = NULL;
    }
    gc_heaps_used = 0;

    for (int j = 0; j <= root_used ; ++j){
        roots[j].ptr = NULL;
        roots[j].optr = NULL;
    }
    root_used = 0;
}
/**
 * 1. 测试条件保证申请内存保证在TINY_HEAP_SIZE下，因为大于这个数会发生gc
 * 测试 分配和 释放是否正常
 */
void test_malloc_free(size_t req_size){
    printf("-----------测试内存申请与释放------------\n");
    printf("-----------***************------------\n");
    //在回收p1的情况下 p2的申请将复用p1的地址
    void *p1 = gc_malloc(req_size);
    gc_free(p1);
    void *p2 = gc_malloc(req_size);
    //在上面 p1被释放了，p2 从新申请 会继续从堆的起始位置开始分配 所以 内存地址是一样的
    assert(p1 == p2);
    gc_free(p2);

    //在不清除p1的情况下 p2 会申请不同内存
    p1 = gc_malloc(req_size);
    p2 = gc_malloc(req_size);
    if(req_size == TINY_HEAP_SIZE)
        assert(p1 == p2);
    else
        assert(p1 != p2);


    printf("-----------   passing     ------------\n\n");
    clear();
}
void test_gc(size_t req_size){
    printf("-----------测试gc         ------------\n");
    printf("-----------***************------------\n");
    void *p1 = gc_malloc(req_size);
    gc();
    void *p2 = gc_malloc(req_size);
    //执行gc后 p1被回收
    assert(p1 == p2);
    clear();

    //因为该变量被root引用 所以不会被gc
    p1 = gc_malloc(req_size);
    add_roots(&p1);
    gc();
    p2 = gc_malloc(req_size);
    assert(p1 != p2);

    printf("-----------   passing     ------------\n\n");
    clear();
}
void test_large_gc(){
    printf("-----------测试大内存gc         ------------\n");
    printf("-----------***************------------\n");
    void *p1 = gc_malloc(TINY_HEAP_SIZE);
    void *p2 = gc_malloc(TINY_HEAP_SIZE);
    //因为 req_size 已经达到堆内存了上限了，且无可用内存 会自动执行gc
    assert(p1 == p2);
    clear();

    p1 = gc_malloc(TINY_HEAP_SIZE);
    //将对象加入root
    add_roots(&p1);
    p2 = gc_malloc(TINY_HEAP_SIZE);
    //p1 不会被回收
    //p2 会申请新的内存
    assert(p1 != p2);

    printf("-----------   passing     ------------\n");
    clear();
}

/**
 * 对引用进行测试
 */
void test_reference_gc()
{
    printf("-----------测试引用gc         ------------\n");
    printf("-----------***************------------\n");
    typedef struct obj{
        int v;
        struct obj* left;
        struct obj* right;
    }Obj;

    Obj* p   = gc_malloc(sizeof(Obj));
    p->v     = 10;
    p->left  = gc_malloc(sizeof(Obj));
    p->right = gc_malloc(sizeof(Obj));
    p->left->v = 11;
    p->right->v = 12;

    //加入root 即使left right 没有加入 但是他们作为 p的子节点引用 会被标记
    add_roots(&p);
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
    for (int i = 0; i < 1000; ++i) {
        int size = rand()%90;
        void *p = gc_malloc(size);
    }
    void *p = gc_malloc(24);
    p = gc_malloc(24);
    p = gc_malloc(24);
    gc();
    end = time(NULL);
    printf("execution seconds:%d\n",difftime(end,start));
}
int  main(int argc, char **argv)
{

    //小内存测试，
    test_malloc_free(8);
    clear();
    //大内存测试
    test_malloc_free(TINY_HEAP_SIZE);
    clear();
    //测试gc
    test_gc(8);
    clear();
    //大内存 无需手动gc 测试
    test_large_gc();
    clear();
    //对象引用测试
    test_reference_gc();
    clear();

//    test_malloc_speed();
    return 0;
}