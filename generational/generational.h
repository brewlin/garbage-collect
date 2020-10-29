/**
 *@ClassName copying
 *@Author brewlin
 *@Date 2020/10/22 0022 上午 11:01
 *@Version 1.0
 **/
#ifndef GC_LEARNING_COPYING_H
#define GC_LEARNING_COPYING_H

#define AGE_MAX 3

//将申请的内存 加入 root 管理
void  add_roots(void* ptr);
//在gc的时候判断该对象是否在old堆上分配的
int   is_pointer_to_old_space(void* ptr);

/**
 * 写入屏障
 * 在更新对象引用的时候 需要判断obj是否是老年代对象  new_obj为年轻代对象
 * 如果是 则需要加入 记忆结果集
 * @param obj
 * @param field
 * @param new_obj
 */
void write_barrier(void *obj_ptr,void *field,void* new_obj_ptr);

//每次gc的时候将 free指向 to的开头
extern void* to_free_p;


/****** 标记清除法实现-------------*/
typedef struct root_range {
    void *start;
    void *end;
    //这里存储了 用户方变量地址 因为复制完成后需要替换用户态的变量地址
    void *optr;
}root;
extern root roots[ROOT_RANGES_LIMIT];
extern size_t root_used;
//保存对象引用关系同时存在新生代和老年代的 对象
extern void* rs[ROOT_RANGES_LIMIT];
extern int   rs_index;

#endif //GC_LEARNING_COPYING_H
