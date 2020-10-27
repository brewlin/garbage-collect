#include "gc.h"
#include "compact.h"

typedef struct t{
    int       value;
    struct t* next;
}T;
int clear(){

    free_list = NULL;
    root_used = 0;
}
void test_auto_gc(){
    //初始化4个T空间大小
    gc_init(2 * (sizeof(T) + HEADER_SIZE), sizeof(T));

    T* t1 = gc_malloc(sizeof(T));
    assert(t1);
    T* t2 = gc_malloc(sizeof(T));
    assert(t2);

    //内存不够了 会自动gc
    T* t4 = gc_malloc(sizeof(T));
    assert(t4);

}
/**
 * 测试加入root后的gc情况
 */
void test_add_root_gc(){
    //初始化4个T空间大小
    gc_init(2 * (sizeof(T) + HEADER_SIZE), sizeof(T));

    T* t1 = gc_malloc(sizeof(T));
    t1->value = 11;
    assert(t1);
    add_roots(&t1);
    T* t2 = gc_malloc(sizeof(T));
    t2->value = 12;
    t1->next = t2;

    //内存不够了 会自动gc 但是gc后依然不够用
    T* t4 = gc_malloc(sizeof(T));
    assert(t4 == NULL);
    assert(t1->value == 11);
    assert(t2->value == 12);

    //将t2去除
    t1->next = NULL;
    //空间不够 会自动gc 释放 t3
    T* t5 = gc_malloc(sizeof(T));
    assert(t1->value == 11);
    assert(t2->value == 0);
    assert(t5);
}
void main(){
    // 测试不加入 root的时候  会自动进行gc复制
    //看看gc压缩的结果是否符合预期
    test_auto_gc();
    clear();

     test_add_root_gc();
}