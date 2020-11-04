[toc]

垃圾回收算法的分析与实现

对相关算法理论翻译为c代码实现
## 保守式gc
当前所有gc算法的实现都是基于`保守式gc`:
```
//在gc过程中重要的一个步骤就是要辨别该内存是 指针 or 非指针:
void* p2 = gc_malloc(size);
obj->value = 0x5555555;
```
1. 当gc的时候找到`p2`并对其`child`递归搜索标记，找到`value`时误判为指针后标记
```
for(child: obj){
    //遍历到value成员时 保守判断为一个正确的指针 被标记了
    mark(child);
}
```
2. 但实际上`value`成员只是一个数值，非指针，导致实际指向的内存得不到释放
3. 要减小这种误判的方式有很多种，现代很多语言都是基于保守式gc，所以肯定有的方式
```
1. 对于ruby 来说在编译期间给每个数值进行标记，在运算之前进行还原
2. 通过将分配的内存进行首地址对齐，然后在辨别指针的时候能够过滤一些
```

## [heaps](./header) 堆
当前的所有gc实现都会提前向操作系统分配一块大内存`sbrk()`，成为独立管理的内存池，所有内存分配其实都是从我们维护的内存池中分配
- gc_malloc 
```c
typedef struct t{
    .....
}T;
//无需其他依赖 可动态分配不同类型大小的内存
T* p = gc_malloc(sizeof(T));
```

另外有一个空闲链表机制`free_list`，将所有的`heap`用单链表串联起来，且在用户释放内存的时候会挂到上面进行重复利用
## roots 根
下面这些空间都是根`roots`
- 寄存器
- 调用栈
- 全局变量空间

对于gc过程来说，只要是能通过根`roots`能够找到的对象都称为`可达对象`.所以这些对象在回收阶段不会被清除，

并非只有上面的空间才能成为根,通常情况下对于动态运行时语言来说，都会在程序层面创建一个集合，然后自己来管理分配的对象，实现了根对象的管理

当前所有gc实现都是模拟的root，通过一个全局数组来模拟root对象

## 各种gc实现
### 标记-清除算法
- [mark-sweep](./src/mark-sweep) 基础实现
- [mark-sweep_multi_free_list](./src/mark-sweep-multi-free-list) 多链表法实现

### 引用计数算法
- [reference-count](./src/refcount) 基础实现

### 复制算法
- [copying](./src/copying) 基础实现
- [copying_or_mark](./src/copying-or-mark) 复制算法+标记清除 组合应用实现

### 压缩算法
- [compact_lisp2](./src/compact-lisp2) lisp2 算法
- [compact_two_finger](./src/compact-two-finger) two_finger 算法

### 分代垃圾回收算法
- [generational](./src/generational)
### 增量式-三色标记算法
- [tri-color-marking](./src/tri-color-marking)