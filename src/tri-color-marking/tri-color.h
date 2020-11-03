/**
 *@ClassName tri
 *@Deacription go
 *@Author brewlin
 *@Date 2020/11/3 0003 下午 1:02
 *@Version 1.0
 **/
#ifndef GC_LEARNING_TRI_COLOR_H
#define GC_LEARNING_TRI_COLOR_H

#include "stack.h"
/*** tri-color-marking ***/
#define GC_ROOT_SCAN 1
#define GC_MARK      2
#define GC_SWEEP     3


extern Stack stack;
extern int gc_phase;
extern int max_mark;
extern int max_sweep;
extern int sweeping;

void write_barrier(void *obj_ptr,void *field,void* new_obj_ptr);
#endif //GC_LEARNING_TRI_COLOR_H
