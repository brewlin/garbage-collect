[toc]

阅读垃圾回收算法的分析与实现书,对相关算法理论翻译为c实现

- [mark-sweep](mark-sweep) 标记清除算法-基础实现
- [mark-sweep_multi_free_list](mark-sweep-multi-free-list) 标记清除算法-多链表法实现
- [reference-count](refcount) 引用计数算法
- [copying](copying) 复制算法-基础实现
- [copying_or_mark](copying-or-mark) 复制算法+标记清除-组合应用实现
- [compact_lisp2](compact-lisp2) 压缩算法-lisp2算法
- [compact_two_finger](compact-two-finger) 压缩算法-two_finger
- [generational](generational) 分代算法-复制+标记清除
- [tri-color-marking](tri-color-marking) 增量式算法-三色标记


- 堆 heaps

当前的所有gc实现都会提前向操作系统分配一块大内存`sbrk()`，成为独立管理的内存池，所有内存分配其实都是从我们维护的内存池中分配
```c
typedef struct t{
    .....
}T;
//无需其他依赖 可动态分配不同类型大小的内存
T* p = gc_malloc(sizeof(T));
```

另外有一个空闲链表机制`free_list`，将所有的`heap`用单链表串联起来，且在用户释放内存的时候会挂到上面进行重复利用

- roots 根

下面这些空间都是根`roots`
```asciidoc
- 寄存器
- 调用栈
- 全局变量空间
```
对于gc过程来说，只要是能通过根`roots`能够找到的对象都称为`可达对象`.所以这些对象在回收阶段不会被清除，

并非只有上面的空间才能成为根,通常情况下对于动态运行时语言来说，都会在程序层面创建一个集合，然后自己来管理分配的对象，实现了根对象的管理

当前所有gc实现都是模拟的root，通过一个全局数组来模拟root对象

