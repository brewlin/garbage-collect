/**
 *@ClassName copying
 *@Author brewlin
 *@Date 2020/10/22 0022 上午 11:01
 *@Version 1.0
 **/
#ifndef GC_LEARNING_COPYING_H
#define GC_LEARNING_COPYING_H

#include "gc.h"

#define AGE_MAX 3

/**
 * 写入屏障
 * 在更新对象引用的时候 需要判断obj是否是老年代对象  new_obj为年轻代对象
 * 如果是 则需要加入 记忆结果集
 * @param obj
 * @param field
 * @param new_obj
 */
void    write_barrier(void *obj_ptr,void *field,void* new_obj_ptr);
void*   major_malloc(size_t req_size);
void*   minor_malloc(size_t req_size);
void    major_gc(void);
void    minor_gc(void);

//生成空间 new generational
extern int newg;
//幸存空间1 survivor1 generational
extern int survivorfromg;
//幸存空间2 survivor1 generational
extern int survivortog;
//老年代空间 old generational
extern int oldg;
//保存对象引用关系同时存在新生代和老年代的 对象
extern void* rs[ROOT_RANGES_LIMIT];
extern int   rs_index;

extern void* to_free_p;
extern Header* new_free_p;

#endif //GC_LEARNING_COPYING_H
