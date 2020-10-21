# [mark_sweep](./mark_sweep)
基础标记 清除算法

# [multi_free_list](./multi_free_list)
为了优化分配内存速度，增加了多个空闲链表的设计
```
一般都是按字节内存 以8 对齐，所以基本只会使用 8 16 。。
list free_list[100]

free_list[1] 表示该链表指向的数据都是 1个字节的内存
free_list[2] 表示该链表指向的数据都是 2个字节的内存
.....
```
通过降低分配内存的查找时间 优化分配速度