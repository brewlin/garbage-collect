#include "gc.h"
#include "copying.h"

typedef struct t{
    int       value;
    struct t* next;
}T;

void test_auto_gc(){
    //分配两个T 空间
    gc_init(2* (sizeof(T) + HEADER_SIZE) );

    T* t1 = gc_malloc(sizeof(T));



}

void main(){
    //初始化堆  from to
    gc_init(TINY_HEAP_SIZE);

    typedef struct obj{
        int v;
        struct obj* left;
    }Obj;

    Obj* p = gc_malloc(sizeof(Obj));
    add_roots(&p);
    printf("o:%p p:%p\n",*(void**)p,p);

    p->left = gc_malloc(sizeof(Obj));
    printf("o:%p p->left:%p\n",*(void**)p->left,p->left);

    p->v = 10;
    p->left->v = 20;

    gc();

    printf("p:%p p->v:%d\n",p,p->v);
    printf(" :%p p->left->v:%d\n",p->left,p->left->v);



}