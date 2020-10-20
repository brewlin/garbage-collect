#include "gc.h"
int clear(){
    free_list = NULL;
    for (int i = 0; i <= gc_heaps_used; ++i){
        gc_heaps[i].size = 0;
        gc_heaps[i].slot = NULL;
    }
    gc_heaps_used = 0;

    for (int j = 0; j <= root_used ; ++j){
        roots[j].start = NULL;
        roots[j].end = NULL;
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
    add_roots(p1,p1+req_size);
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
    add_roots(p1,p1+TINY_HEAP_SIZE);
    p2 = gc_malloc(TINY_HEAP_SIZE);
    //p1 不会被回收
    //p2 会申请新的内存
    assert(p1 != p2);

    printf("-----------   passing     ------------\n");
    clear();
}




int  main(int argc, char **argv)
{
    //小内存测试，
    test_malloc_free(8);
    //大内存测试
    test_malloc_free(TINY_HEAP_SIZE);
    //测试gc
    test_gc(8);
    test_large_gc();
    return 0;
}