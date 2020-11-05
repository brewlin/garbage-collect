#include "gc.h"
typedef struct t{
    int       value;
    struct t* next;
}T;
int clear(){
    free_list = NULL;

    for (int j = 0; j <= root_used ; ++j){
        roots[j].ptr = NULL;
    }
    root_used = 0;
}
void test_auto_gc(){
    //分配两个T 空间
    gc_init(2* (sizeof(T) + HEADER_SIZE) );

    T* t1 = gc_malloc(sizeof(T));
    t1->value = 1;
    T* t2 = gc_malloc(sizeof(T));
    t2->value = 2;
    assert(t1!=NULL);
    assert(t2!=NULL);
    assert(t1!=t2);

    //因为 t1 t2 没有加入root
    //所以在内存不够的时候 会自动gc 进行复制 清除之前的数据
    T* t3 = gc_malloc(sizeof(T));
    //t1 t2 都被回收了
    assert(t1->value == 0);
    assert(t2->value == 0);
    //t1 是属于 from堆分配的
    //t3 是属于gc后 to堆分配的，如果gc成功后，t3应该不等于t1
    assert(t3 != NULL);
    assert(t3 != t1);
}
/**
 * 加入root后的 gc测试
 */
void test_add_root_gc(){
    //初始化堆  from to
    gc_init(TINY_HEAP_SIZE);

    T* p = gc_malloc(sizeof(T));
    T* backup_p = p;
    add_roots(&p);
    p->next = gc_malloc(sizeof(T));
    p->value = 10;
    p->next->value = 20;
    T* backup_next = p->next;
    //手动触发gc 将 p 和 next 从 from 拷贝到 to
    gc();

    //源地址发生改变 p != p
    assert(p != backup_p);
    assert(p->next != backup_next);
    //原先的值保持不变
    assert(p->value == 10);
    assert(p->next != NULL);
    assert(p->next->value == 20);



}
void main(){
    // 测试不加入 root的时候  会自动进行gc复制
    //看看gc复制的结果是否符合预期
    test_auto_gc();
    clear();

    //测试加入root gc复制后 用户态变量地址是否改变
    /**
     * void *p = gc_malloc()
     * gc();
     * p != p
     */
     test_add_root_gc();
}