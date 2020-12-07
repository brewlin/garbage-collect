#include "gc.h"
#include "root.h"
#include "Hugmem.h"

/**
 * 对该对象进行标记
 * 并进行子对象标记
 * @param ptr
 */
int gc_mark(void * ptr)
{
    if (ptr == NULL)return TRUE;

    poolp pool;
    uint size;
    pool = POOL_ADDR(ptr);
    if((uptr)pool < arenas[0].address || (uptr)pool > (arenas[0].address + ARENA_SIZE)) {
        // mark(&Hugmem,ptr);
        return NOT_STACK;
    }
    if (!Py_ADDRESS_IN_RANGE(ptr, pool)) {
        // mark(&Hugmem,ptr);
        return NOT_STACK;
        // return;
    }

    size = INDEX2SIZE(pool->szidx);

    Header *hdr = ptr - 8;
    if (!FL_TEST(hdr->flags, FL_ALLOC)) {
//      printf("flag not set alloc\n");
      return TRUE;
    }
    if (FL_TEST(hdr->flags, FL_MARK)) {
      //printf("flag not set mark\n");
      return TRUE;
    }
    //printf("marking %p:%d ",ptr,*(int*)ptr);
    /* marking */
    FL_SET(hdr->flags, FL_MARK);

    //进行child 节点递归 标记
    for (void* p = ptr; p < (ptr + size -8); p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        gc_mark(*(void **)p);
    }
    return TRUE;
}
/**
 * 清除 未标记内存 进行回收利用
 */
void     gc_sweep(void)
{
    //遍历areans_object
    struct arena_object* area = &arenas[0];
    poolp  pool;
    uint   size = 0;

    //从 first_pools - pool_adress 之间遍历
    for (void* p = area->first_address; p < (void*)area->pool_address; p += POOL_SIZE)
    {
        pool = (poolp)p;
        size = INDEX2SIZE(pool->szidx);
        void* start_addr = p + POOL_OVERHEAD;
        void* end_addr = p + POOL_SIZE;
        for (void *pp = start_addr; pp < end_addr; pp += size)
        {
            Header *obj = (Header*)pp;
            //查看该堆是否已经被使用
            if (FL_TEST(obj->flags, FL_ALLOC)) {
                //查看该堆是否被标记过
                if (FL_TEST(obj->flags, FL_MARK)) {
//                    DEBUG(printf("解除标记 : %p\n", p));
                    // printf("解除标记 : %p\n", p);
                    FL_UNSET(obj->flags, FL_MARK);
                }else {
//                    DEBUG(printf("清除回收 :\n"));
                    //printf("清除回收 %d:%d ",*(int*)(pp +8),obj->flags);
                    FL_UNSET(obj->flags, FL_ALLOC);
                    Free(pp);
                }
            }

        }


    }
}
void tell_is_stackarg(void* arg){
    void *top = get_sp();
    if(sp_start > arg && arg > top){
        if(gc_mark(*(void**)arg) == NOT_STACK){
            mark(&Hugmem,*(void**)arg);
        }
    }
}
/**
 * 寄存器扫描
 */
void scan_register()
{
//    printf("[gc] start scan register\n");
    void *reg;
    if(reg = get_sp())  tell_is_stackarg(reg);
    if(reg = get_bp())  tell_is_stackarg(reg);
    if(reg = get_di())  tell_is_stackarg(reg);
    if(reg = get_si())  tell_is_stackarg(reg);
    if(reg = get_dx())  tell_is_stackarg(reg);
    if(reg = get_cx())  tell_is_stackarg(reg);
    if(reg = get_r8())  tell_is_stackarg(reg);
    if(reg = get_r9())  tell_is_stackarg(reg);
    if(reg = get_ax())  tell_is_stackarg(reg);
    if(reg = get_bx())  tell_is_stackarg(reg);
}
/**
 * 栈扫描
 */
void scan_stack(){
//    printf("[gc] start scan stack\n");
    //现在开始是真正的扫描系统栈空间
    void * cur_sp = get_sp();
    //高低往低地址增长
    assert(sp_start >= cur_sp);
    for (; cur_sp < sp_start ; cur_sp += 4){
        if(gc_mark(*(void**)cur_sp) == NOT_STACK){
            mark(&Hugmem,*(void**)cur_sp);
        }
    }
}
/**
 * 标记清除算法的gc实现
 */
void  gc(void)
{
    // printf("[gc] start gc\n");
    scan_register();
    scan_stack();

    //标记完成后 在进行 清除 对于没有标记过的进行回收
    gc_sweep();
}

