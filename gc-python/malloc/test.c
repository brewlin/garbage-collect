#include "python.h"

int main()
{

    clock_t start, finish;
    double  duration;
    start = clock();
    for (int i = 0; i < 100000; ++i) {
        int size = rand()%90;
        void* p = PyObject_Malloc(size);
//        PyObject_Free(p);
    }
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "%f seconds\n", duration );
    return 0;
}