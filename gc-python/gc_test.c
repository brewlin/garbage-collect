#include "gc.h"
#include "root.h"
#include "Array.h"
#include <stdio.h>
#include "help.h"

typedef struct obj
{
    int a;
    int b;
    struct obj *next;
}Obj;
Obj *t;
int __failed_tests = 0;
int __test_num = 0;

void array_test()
{
    array_t* arr = array_create(8,sizeof(Obj*));
    assert_s("[arr_create]",arr != NULL)
    for (int i = 0; i < 1000; ++i) {
        Obj** tmp = array_push(arr);
        *tmp = gc_malloc(sizeof(Obj));
        (*tmp)->a = i;
        (*tmp)->b = i;
    }
    Obj** index = arr->addr;
    gc();
    for(int i = 0 ; i < 1000; ++i){
        assert(index[i]->a == i);
        assert(index[i]->b == i);
    }
    gc();

}
int test_speed()
{
    clock_t start, finish;
    double  duration;
    start = clock();
    for (int i = 0; i < 1000000; ++i) {
        int size = rand()%90;
        void* p = gc_malloc(size);
        *(int*)p = size;
    }
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    assert_s("[speed-test]",duration)
    printf( "%f seconds\n", duration );
    return 0;
}

void test_logic(){
    Obj* p1 = gc_malloc(sizeof(Obj));
    p1->a = 10;
    p1->b = 20;
    gc();
    assert_s("[logic] test stack",p1->a == 10);
    assert_s("[logic] test p->b == 20",p1->b == 20);
    p1->next = gc_malloc(sizeof(Obj));
    assert_s("[logic] test p1->next",p1->next);
    p1->next->a = 22;
    p1->next->b = 33;
    gc();
    assert_s("[logic] p1->next->a",p1->next->a == 22);
    assert_s("[logic] p1->next->b",p1->next->b == 33);
}
void main(){
    sp_start = get_sp();
    array_test();
    test_speed();
    test_logic();
    test_report();
}
