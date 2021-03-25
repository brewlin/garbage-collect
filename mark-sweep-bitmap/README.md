[系列实现分析文档](https://wiki.brewlin.com/wiki/blog/gc-learning/GC%E7%AE%97%E6%B3%95%E5%88%86%E6%9E%90%E4%B8%8E%E5%AE%9E%E7%8E%B0/)

1. 位图法不需要在对象头添加附加信息，所以不能公用gc.h实现
2. 规定 1个bit位 标记 对应8字节64位的对象信息
