# gc-learning
<p>
<img alt="GitHub" src="https://img.shields.io/github/license/brewlin/gc-learning">
<img alt="GitHub code size in bytes" src="https://img.shields.io/github/languages/code-size/brewlin/gc-learning">
</p>

垃圾回收算法的分析与实现,基于c实现各系列gc [docs:分析文档](https://wiki.brewlin.com/wiki/blog/gc-learning/GC%E7%AE%97%E6%B3%95%E5%88%86%E6%9E%90%E4%B8%8E%E5%AE%9E%E7%8E%B0/)

```asciidoc
> dos2unix auto_test.sh
> sh auto_test.sh
```
## @demo
```c
#include "gc.h"

Obj* p = gc_malloc(sizeof(Obj));
// don't need care the `free`,let it go

```

## @analysis
- [x] python的gc实现
- [ ] 实现一个可生产的gc


## @mark-sweep
- [x] 标记清除算法-基础实现
- [x] 标记清除算法-多链表法实现

## @reference-count 
- [x] 引用计数算法

## @copying
- [x] 复制算法-基础实现
- [x] 复制算法+标记清除-组合应用实现

## @compact
- [x] 压缩算法-lisp2算法
- [x] 压缩算法-two_finger

## @generational
- [x] 分代算法-复制+标记清除

## @incremental
- [x] 增量式算法-三色标记





