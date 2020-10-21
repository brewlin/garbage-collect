#ifndef _REFCOUNT_H
#define _REFCOUNT_H
void  gc_inc(void *ptr);
void  gc_dec(void *ptr);
int   ref_count(void *ptr);

#endif
