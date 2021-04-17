/**
 *@ClassName array
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 下午 4:16
 *@Version 1.0
 **/
#include "array.h"
#include "sysapi.h"
#include "gc.h"


int_t array_init(array_t *array, uint_t n, size_t size)
{
    array->used = 0;
    array->size = size;
    array->total = n;

    array->addr = sys_fixalloc(n * size,ptrSize);
    if (array->addr == NULL) {
        return ERROR;
    }

    return OK;
}

void array_destroy(array_t *a)
{
//    free(a);
}


void *array_push(array_t *a)
{
    void        *elt, *new;
    size_t       size;

    if (a->used == a->total) {

        // 数组满了
        size = a->size * a->total;
        //直接扩充2倍大小
        new = sys_fixalloc(2 * size,ptrSize);
        if (new == NULL) {
            printf("[arr_pushn] failed to expand memeory\n");
            return NULL;
        }

        memcpy(new, a->addr, size);
        // free(a->addr);
        a->addr = new;
        a->total *= 2;
    }

    elt = (u_char *) a->addr + a->size * a->used;
    a->used++;
    return elt;
}
