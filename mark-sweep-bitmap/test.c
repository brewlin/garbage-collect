#include "gc.h"

int clear(){
    for (int j = 0; j <= root_used ; ++j){
        roots[j].ptr = NULL;
        roots[j].optr = NULL;
    }
    root_used = 0;
}
void test_malloc_speed(){
    clock_t start,end;
    double  duration;
    start = clock();

    for (int i = 0; i < 100; ++i) {
        int size = rand()%90;
        void *p = gc_malloc(size);
        if(!p){
//            printf("even gc(),still out of memory!\n");
            //清除根 达到回收全部对象的效果
            clear();
        }else{
            add_roots(p);
        }
//        gc_free(p);
    }
    for (int i = 0; i < 100; ++i) {
        int size = rand() % 90;
        void *p = gc_malloc(size);
        if(!p) {
            printf("even gc(),still out of memory!\n");
        }
    }
    end = clock();
    duration = (double)(end - start)/CLOCKS_PER_SEC;
    printf("execution seconds:%f\n",duration);
}
int  main(int argc, char **argv)
{
    gc_init();

    test_malloc_speed();
    return 0;
}