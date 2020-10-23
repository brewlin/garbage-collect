gc 复制算法


# add_root
这里和其他算法的`add_root`有点不一样，这里的参数不在是对象锁指向的地址
而是对象本身的地址
```asciidoc
void *p = gc_malloc(sizeof(...));
add_root(&p);
```
因为gc复制涉及到内存的拷贝，所以`p`所指向的内存也需要改变

因为发送gc后将`from` 拷贝到 `to` 那么原先用户态的指针依然会执行from
所以需要更改`p`执向到新地址