
#ifndef _ARRAY_H_INCLUDED_
#define _ARRAY_H_INCLUDED_

#include <string.h>
#define TRUE   1
#define FALSE  0
#define OK     0
#define ERROR  -1
typedef int  int_t;
#define ushort_t  u_short
#define uint_t    int
#define ARRAY_SIZE 8

typedef struct {
    void     *addr;
    uint_t   used;
    size_t   size;
    uint_t   total;
} array_t;



array_t *array_create(uint_t n, size_t size);
void array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_push_n(array_t *a, uint_t n);

#endif /* _ARRAY_H_INCLUDED_ */
