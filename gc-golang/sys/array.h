/**
 *@ClassName array
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 4:16
 *@Version 1.0
 **/
#ifndef GOC_ARRAY_H
#define GOC_ARRAY_H

#include "stdio.h"


typedef unsigned char      u_char;
typedef int                int_t;
typedef unsigned int       uint_t;
typedef unsigned short int u_short;



typedef struct {
    void     *addr;
    uint_t   used;
    size_t   size;
    uint_t   total;
} array_t;
typedef array_t array;

int_t array_init(array_t *array, uint_t n, size_t size);
void  array_destroy(array_t *a);
void *array_push(array_t *a);



#endif //GOC_ARRAY_H
