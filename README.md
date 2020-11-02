[toc]
垃圾回收算法的分析与实现

![image](https://note.youdao.com/yws/res/30095/46183C4326194C829FA7E905CF1ED3FC)

对相关算法理论翻译为c代码实现
# 基础概念
当前所有gc算法的实现都是基于`保守式gc`:
```
//在gc过程中重要的一个步骤就是要辨别该内存是 指针 or 非指针:
long  p1 = 0x55555555;

//p2 = 0x5555555;
void* p2 = gc_malloc();
obj->value = p1;
//上面的情况就是p2指针实际是没有被引用的，但是在gc过程中
for(child: obj){
    //这个时候遍历到了value = 0x55555555;的时候误以为是p2指针
    //结果p2指针本来应该被回收的（因为没有被任何根引用）
    //但是因为保守式gc 导致被误判了，p2得不到回收
    mark(child);
}




```
## heaps 堆
当前的所有gc实现都会提前向操作系统分配一块大内存`sbrk()`，成为独立管理的内存池，所有内存分配其实都是从这块用户态堆上申请
- gc_malloc 
```c
typedef struct t{
    .....
}T;
//无需其他依赖 可动态分配不同类型大小的内存
T* p = gc_malloc(sizeof(T));
```
## roots 根
下面这些空间都是根`roots`
- 寄存器
- 调用栈
- 全局变量空间

对于gc过程来说，只要是能通过根`roots`能够找到的对象都称为`可达对象`.所以这些对象在回收阶段不会被清除，

并非只有上面的空间才能成为根,通常情况下对于动态运行时语言来说，都会在程序层面创建一个集合，然后自己来管理分配的对象，实现了根对象的管理



# 标记-清除算法

## [mark-sweep](src/mark-sweep) 
基础实现
## [mark-sweep_multi_free_list](src/mark-sweep_multi_free_list) 
多链表法实现

# 引用计数算法
## [reference-count](src/refcount) 
基础实现

# 复制算法
## [copying](src/copying) 
基础实现
## [copying_or_mark](src/copying_or_mark) 
复制算法+标记清除 组合应用实现

# 压缩算法
## [compact_lisp2](src/compact_lisp2) 
lisp2 算法
## [compact_two_finger](src/compact_two_finger) 
two_finger 算法

# 分代垃圾回收算法
[generational](src/generational)
# 增量式-三色标记算法
[tri-color-marking](src/tri-color-marking)