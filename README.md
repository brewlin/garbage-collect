# gc-learning
garbage collector  for c
# demo
- gc_malloc 内存分配
```c
typedef struct t{
    .....
}T;
//无需其他依赖 可动态分配不同类型大小的内存
T* p = gc_malloc(sizeof(T));
```
- gc   手动执行gc 回收
```c
//一般内存不够的时候会自动进行gc()
T* p = gc_malloc(sizeof(T));
if(!p) gc();

p = gc_malloc(sizeof(T));
```

# 一、gc 基础库部分
## root
## heaps

# 二、各种gc算法实现
## mark sweep 标记清除算法
### [mark-sweep](./mark-sweep) 基础实现
### [mark-sweep_multi_free_list](./mark-sweep_multi_free_list) 多链表法实现

# reference-count 引用计数
## [reference-count](./refcount) 基础实现

## copying 复制算法
### [copying](./copying) 基础实现
### [copying_or_mark](./copying_or_mark) 复制算法+标记清除 组合应用实现

## compact 压缩算法
### [compact_lisp2](./compact_lisp2) lisp2 算法
### [compact_two_finger](./compact_two_finger) two_finger 算法

