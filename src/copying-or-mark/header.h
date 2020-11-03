/**
 *@ClassName header
 *@Deacription go
 *@Author brewlin
 *@Date 2020/11/2 0002 下午 4:50
 *@Version 1.0
 **/
#ifndef GC_LEARNING_HEADER_H
#define GC_LEARNING_HEADER_H

extern void* free_p;
extern int from;
extern int to;

void* gc_mark(void* ptr);
void  gc_sweep(void);

void*   gc_mark_or_copy(void* ptr);
void*   gc_copy(void *ptr);
int     is_pointer_to_from_space(void* ptr);
void    remove_from();
Header* get_header_by_from(void *ptr);
void    copy_reference();

#endif //GC_LEARNING_HEADER_H
