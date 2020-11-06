#include "gc.h"
#include "root.h"

extern void* sp_start;


typedef struct obj{
   int         value;
   struct obj* next;
}Obj;


Obj *p1;
Obj *p2;
Obj *p3;


void f3(){
    Obj* tp3 = gc_malloc(sizeof(Obj));
    p3 = tp3;
    p3->value = 3;
}
void f2(){
    Obj* tp2 = gc_malloc(sizeof(Obj));
    p2 = tp2;
    p2->value = 2;

    f3();
    gc();
    assert(p3->value == 0);
    assert(p2->value == 2);
    assert(p1->value == 1);
}
void f1(){
    Obj* tp1 = gc_malloc(sizeof(Obj));
    p1 = tp1;
    p1->value = 1;

    f2();
    gc();
    assert(p3->value == 0);
    assert(p2->value == 0);
    assert(p1->value == 1);
}
int  main(int argc, char **argv)
{
    //标记栈起始位置
    sp_start = get_sp();
    f1();
    gc();
    assert(p3->value == 0);
    assert(p2->value == 0);
    assert(p1->value == 0);

    return 0;
}