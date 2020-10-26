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
    gc_init(4 * (sizeof(T) + HEADER_SIZE));

    T* t1 = gc_malloc(sizeof(T));
    assert(t1);
    T* t2 = gc_malloc(2 * sizeof(T));
    assert(t2);
    T* t3 = gc_malloc(sizeof(T));
    assert(t3);

    //内存不够了 会自动gc
    T* t4 = gc_malloc(sizeof(T));
    assert(t4);

}
/**
 * 测试加入root后的gc情况
 */
void test_add_root_gc(){
    //初始化4个T空间大小
    gc_init(4 * (sizeof(T) + HEADER_SIZE));

    T* t1 = gc_malloc(sizeof(T));
    t1->value = 11;
    assert(t1);
    add_roots(&t1);
    T* t2 = gc_malloc(2 * sizeof(T));
    t2->value = 12;
    T* t3 = gc_malloc(sizeof(T));
    t1->next = t2;
    t2->next = t3;
    t3->value = 13;

    //内存不够了 会自动gc 但是gc后依然不够用
    T* t4 = gc_malloc(sizeof(T));
    assert(t4 == NULL);
    assert(t1->value == 11);
    assert(t2->value == 12);
    assert(t3->value == 13);

    //将t3去除
    t2->next = NULL;
    //空间不够 会自动gc 释放 t3
    T* t5 = gc_malloc(sizeof(T));
    assert(t1->value == 11);
    assert(t2->value == 12);
    assert(t3->value == 0);
    assert(t5);
}
void main(){
    // 测试不加入 root的时候  会自动进行gc复制
    //看看gc复制的结果是否符合预期
    test_auto_gc();
    clear();

     test_add_root_gc();
}