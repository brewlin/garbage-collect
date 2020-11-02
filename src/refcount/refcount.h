#ifndef _REFCOUNT_H
#define _REFCOUNT_H
void  gc_inc(void *ptr);
void  gc_dec(void *ptr);
void  gc_update(void *ptr,void *obj);
int   ref_count(void *ptr);

#endif
