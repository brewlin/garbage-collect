#include "gc.h"
#include "copying.h"

typedef struct t{
    int       value;
    struct t* next;
}T;
int clear(){
    free_list = NULL;
    to = 0;
    from = 1;
    for (int j = 0; j <= root_used ; ++j){
        roots[j].start = NULL;
        roots[j].end = NULL;
    }
    gc_heaps_used = 0;
    free_p = NULL;
    root_used = 0;
}
void test_auto_gc(){
    /**
     * 1. 每个堆 一个 T 空间大小
     * 2. 总共3个堆
     *          heaps[0] 作为to
     *          heaps[1] 作为from
     *          heaps[2] 作为mark1
     */
    gc_init(sizeof(T) + HEADER_SIZE,3);
    assert(to == 0);
    assert(from == 1);

    //heaps[1] from分配完了
    T* t1 = gc_malloc(sizeof(T));
    t1->value = 1;
    //heaps[2] mark分配完了
    T* t2 = gc_malloc(sizeof(T));
    t1->next = t2;
    t2->value = 2;
    assert(t1!=NULL);
    assert(t2!=NULL);
    assert(t1!=t2);

    //会自动运行gc
    //from和 mark1 被回收
    //to   变成 heaps[1]
    //from 变成 heaps[2]
    //mark 变成 heaps[0]
    T* t3 = gc_malloc(sizeof(T));
    t3->value = 100;
    assert(to == 1);
    assert(from == 2);
    //t1 t2 都被回收了
    //因为gc的时候先将 gc_heaps[3] 进行标记清除，且原本的t2 是从gc_heaps[3]分配的
    //所以再次分配的时候 由于空闲链表的设计 t3 依然先从heaps[3]分配，所以 t3==t2
    assert(t3 == t2);

    //上面gc的时候 默认情况了两个 T的空间，t3用了一个 那么还剩下一个
    //所以正常情况下是还能分配一个
    T* t4 = gc_malloc(sizeof(T));
    assert(t3->value==100);



}
/**
 * 测试加入root后的gc情况
 */
void test_add_root_gc(){
    /**
     * 1. 每个堆 一个 T 空间大小
     * 2. 总共3个堆
     *          heaps[0] 作为to
     *          heaps[1] 作为from
     *          heaps[2] 作为mark1
     */
    gc_init(sizeof(T) + HEADER_SIZE,3);
    assert(to == 0);
    assert(from == 1);

    //heaps[1] from分配完了
    T* t1 = gc_malloc(sizeof(T));
    t1->value = 1;
    add_roots(&t1);
    //heaps[2] mark分配完了
    T* t2 = gc_malloc(sizeof(T));
    t1->next = t2;
    t2->value = 2;
    assert(t1!=NULL);
    assert(t2!=NULL);
    assert(t1!=t2);

    //会自动运行gc
    //from和 mark1 被回收
    //to   变成 heaps[1]
    //from 变成 heaps[2]
    //mark 变成 heaps[0]
    //因为t1 加入root
    //且t2 被t1引用 所以不会释放任何内存
    //那么t3就会分配失败
    T* t3 = gc_malloc(sizeof(T));
    //t3 分配失败 内存不够
    assert(t3 == NULL);
    assert(to == 1);
    assert(from == 2);
    //t1 t2 都被回收了
    //因为gc的时候先将 gc_heaps[3] 进行标记清除，且原本的t2 是从gc_heaps[3]分配的
    //所以再次分配的时候 由于空闲链表的设计 t3 依然先从heaps[3]分配，所以 t3==t2


    //删除 root 后 可以继续分配
    root_used --;
    T* t4 = gc_malloc(sizeof(T));
    //t1 t2 都被回收
    //t4可以分配
    assert(t4 != NULL);
}
void main(){
    // 测试不加入 root的时候  会自动进行gc复制
    //看看gc复制的结果是否符合预期
    test_auto_gc();
    clear();
    test_add_root_gc();

}