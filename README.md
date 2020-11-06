
阅读垃圾回收算法的分析与实现书,对相关算法理论翻译为c实现
```c
void* p = gc_malloc(size);
gc();
```
- [mark-sweep](mark-sweep) 标记清除算法-基础实现
- [mark-sweep_multi_free_list](mark-sweep-multi-free-list) 标记清除算法-多链表法实现
- [reference-count](refcount) 引用计数算法
- [copying](copying) 复制算法-基础实现
- [copying_or_mark](copying-or-mark) 复制算法+标记清除-组合应用实现
- [compact_lisp2](compact-lisp2) 压缩算法-lisp2算法
- [compact_two_finger](compact-two-finger) 压缩算法-two_finger
- [generational](generational) 分代算法-复制+标记清除
- [tri-color-marking](tri-color-marking) 增量式算法-三色标记

相关文档
- [保守式gc](conservative.md) 关于保守式gc说明
- [heap](heap.md) 关于上面算法的堆依赖介绍
- [root](root.md) 关于根的介绍

其他源码阅读与实现
- [gc-python](gc-python) python的gc实现
- [gc-try](gc-try) 实现一个可生产的gc




