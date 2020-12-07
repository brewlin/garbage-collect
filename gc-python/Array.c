#include "Array.h"
#include "gc.h"

int_t array_init(array_t *array, uint_t n, size_t size)
{
    array->used = 0;
    array->size = size;
    array->total = n;

    array->addr = gc_malloc(n * size);
    if (array->addr == NULL) {
        return ERROR;
    }

    return OK;
}
array_t *array_create(uint_t n, size_t size)
{
    array_t *a;

    a = gc_malloc(sizeof(array_t));
    if (a == NULL) {
        printf("[arr_create] failed to create\n");
        return NULL;
    }

    if (array_init(a,  n, size) != OK) {
        printf("[arr_init] failed to init\n");
        return NULL;
    }

    return a;
}


void array_destroy(array_t *a)
{
    //TODO:释放array
    gc_free(a);
}


void *array_push(array_t *a)
{
    void        *elt, *new;
    size_t       size;

    if (a->used == a->total) {

        // 数组满了
        size = a->size * a->total;
        //直接扩充2倍大小
        new = gc_malloc(2 * size);
        if (new == NULL) {
            printf("[arr_pushn] failed to expand memeory\n");
            return NULL;
        }
        memcpy(new, a->addr, size);
        //手动释放之前决定不会用到的数组 降低gc压力
        gc_free(a->addr);
        a->addr = new;
        a->total *= 2;
    }

    elt = (u_char *) a->addr + a->size * a->used;
    a->used++;
    return elt;
}


void *
array_push_n(array_t *a, uint_t n)
{
    void        *elt, *new;
    size_t       size;
    uint_t   total;

    size = n * a->size;

    if (a->used + n > a->total) {
        //数组满了

        total = 2 * ((n >= a->total) ? n : a->total);

        new = gc_malloc(total * a->size);
        if (new == NULL) {
            printf("[arr_pushn] failed to expand memeory");
            return NULL;
        }
        memcpy(new, a->addr, a->used * a->size);
        //手动释放拷贝前的数组降低gc压力
        gc_free(a->addr);
        a->addr = new;
        a->total = total;
    }
    elt = (u_char *) a->addr + a->size * a->used;
    a->used += n;

    return elt;
}
