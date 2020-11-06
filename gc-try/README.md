# 栈空间扫描 实现root的访问
```asciidoc
void func(){
    void *p = gc_malloc(...);
}
void main(){
    func();
}
```

1. 基于栈扫描的方式遍历可达对象
2. 不需要手动管理root，在gc的时候 会从该`寄存器rsp`执行的栈顶 遍历至`main`的栈地址
3. 从而实现可达对象的标记