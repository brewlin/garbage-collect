/**
 *@ClassName copying
 *@Author brewlin
 *@Date 2020/10/22 0022 上午 11:01
 *@Version 1.0
 **/
#ifndef GC_LEARNING_COPYING_H
#define GC_LEARNING_COPYING_H


//将申请的内存 加入 root 管理
void add_roots(void* ptr);
//进行标记 或者 拷贝遍历
void* gc_mark_or_copy_range(void *start, void *end);
//在gc的时候判断该对象是否在from堆上分配的
int   is_pointer_to_from_space(void* ptr);

//每次gc的时候将 free指向 to的开头
extern void* free_p;


/****** 标记清除法实现-------------*/
typedef struct root_range {
    void *start;
    void *end;
    //这里存储了 用户方变量地址 因为复制完成后需要替换用户态的变量地址
    void *optr;
}root;
extern root roots[ROOT_RANGES_LIMIT];
extern size_t root_used;

#endif //GC_LEARNING_COPYING_H
