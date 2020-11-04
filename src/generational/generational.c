#include "gc.h"
#include "generational.h"

void* rs[ROOT_RANGES_LIMIT];
int   rs_index = 0;

//生成空间 new generational
int newg          = 0;
//幸存空间1 survivor from generational
int survivorfromg = 1;
//幸存空间2 survivor to generational
int survivortog   = 2;
//老年代空间 old generational
int oldg          = 3;

Header* new_free_p;

//每次复制前将 该指针 指向 to的首地址
void* to_free_p;
/**
 * 初始化 4个堆
 * 1个生成空间
 * 2个幸存空间
 * 1个老年代空间
 **/
void gc_init(size_t heap_size)
{
    //关闭自动扩充堆
    auto_grow = 0;

    //gc_heaps[3] 用于老年代堆区
    gc_heaps_used = 3;
    for (size_t i = 0; i < 4; i++){
        //使用sbrk 向操作系统申请大内存块
        void* p = sbrk(heap_size + PTRSIZE);
        if(p == NULL)exit(-1);

        gc_heaps[i].slot = (Header *)ALIGN((size_t)p, PTRSIZE);
        gc_heaps[i].size = heap_size;
        gc_heaps[i].slot->size = heap_size;
        gc_heaps[i].slot->next_free = NULL;
    }
    //初始化新生代空闲链表指针
    new_free_p = gc_heaps[newg].slot;
    //老年代分配需要用到空闲列表 通过gc_free 挂到空闲列表即可
    gc_free(gc_heaps[oldg].slot + 1);
}
/**
 * 从新生代 晋升为 老年代
 * @param ptr
 * @return
 */
void promote(void *ptr)
{
    Header* obj = CURRENT_HEADER(ptr);
    //1 从老年代空间分配出一块 内存 (老年代堆 完全采用 gc标记-清除算法来管理)
    void* new_obj_ptr = major_malloc(CURRENT_HEADER(ptr)->size);
    if(new_obj_ptr == NULL) abort();

    Header* new_obj   = CURRENT_HEADER(new_obj_ptr);
    //将obj 拷贝到 new_obj中
    memcpy(new_obj,obj,obj->size);

    obj->forwarding = new_obj;
    //标志已经复制过了 forwarded = true
    FL_SET(obj,FL_COPIED);

    //for child: obj 这里是为了检查老年代对象是否有对象依然指向新生代中
    for (void *p = ptr; p < (void*)NEXT_HEADER(obj); p++) {

        //解引用 如果该内存依然是指向的from，且有forwarding 则需要改了
        void *ptr = *(void**)p;
        GC_Heap *gh;
        Header *hdr;
        /* mark check */
        if (!(gh = is_pointer_to_heap(ptr))) continue;
        //查看该引用是否存在于 新生代 这里的get_header 只会去查找 新生代和两个幸存代
        if (!(hdr = get_header(gh,ptr)))     continue;
        //存在就要将 new_obj 加入集合
        rs[rs_index++] = new_obj_ptr;
        break;
    }

}
/**
 * 进行子对象拷贝
 */
void* gc_copy(void *ptr){
    GC_Heap *gh;
    Header *hdr;
    /* mark check */
    if (!(gh = is_pointer_to_heap(ptr)))
        return NULL;
    //查看该引用是否存在于 新生代 这里的get_header 只会去查找 新生代和两个幸存代
    if (!(hdr = get_header(gh,ptr))) {
//      printf("not find header\n");
      return NULL;
    }
    assert(FL_TEST(hdr,FL_ALLOC));

    //没有复制过  0
    if(!IS_COPIED(hdr)){
        //判断年龄是否小于阈值
        if(hdr->age < AGE_MAX)
        {
            //TODO: to空间不够了之后 可以将这些数据复制到 老年代空间去
            if(hdr->size > ((Header*)to_free_p)->size){
                printf("to 空间不够了 gc 复制阶段错误\n");
                abort();
            }

            //计算复制后的指针
            Header *forwarding = (Header*)to_free_p;
            //在准备分配前的总空间
            size_t total = forwarding->size;
            //分配一份内存 将源对象拷贝过来
            memcpy(forwarding, hdr, hdr->size);
            //拷贝过后 将原对象标记为已拷贝 为后面递归引用复制做准备
            FL_SET(hdr,FL_COPIED);
            //更新拷贝过后新对象的年龄
            forwarding->age ++;

            //free 指向下一个 body
            to_free_p +=  hdr->size;
            //free_p 执行的剩余空间需要时刻维护着
            ((Header*)to_free_p)->size = total - hdr->size;
            //源对象 保留 新对象的引用
            hdr->forwarding = forwarding;

            printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);
            //从forwarding 指向的空间开始递归
            //递归子对象进行复制
            for (void* p = (void*)(forwarding + 1) ; p < (void*)NEXT_HEADER(forwarding);p++){
                //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
                //递归进行 引用的拷贝
                void *exist  = gc_copy(*(void **)p);
                if(exist){
                    *(Header**)p = exist;
                }
            }
            //返回body
            return forwarding + 1;
        }else{
        //需要晋升为老年代
            promote(ptr);
        }

    }
    //forwarding 是带有header头部的，返回body即可
    return hdr->forwarding + 1;
}
/**
 * 更新所有跨老年代和 新生代的 引用
 */
void update_reference()
{
    //rs 里的基本都是老年代
    for(int i = 0; i < rs_index; i ++)
    {
        void* start =  rs[i];

        //判断对该对象递归引用拷贝后，是否还有继续在新生代的引用
        int has_new_obj = 0;
        //可能申请的内存 里面又包含了其他内存
        for (void *p = start; p < (void*)NEXT_HEADER(CURRENT_HEADER(start)); p++) {

            void *ptr = *(void**)p;
            GC_Heap *gh;
            Header *hdr;
            /* mark check */
            if (!(gh = is_pointer_to_heap(ptr))) continue;
            //如果 child 引用属于新生代 则需要 进行拷贝
            if (hdr = get_header(gh,ptr))
            {
                void* new_obj = gc_copy(ptr);
                printf("拷贝引用 hdr:%p new_obj:%p\n",hdr, new_obj);
                *(Header**)p = new_obj;


                if (!(gh = is_pointer_to_heap(new_obj))) continue;
                //判断拷贝过后的 new_obj 是否还是继续属于新生代
                if (hdr = get_header(gh,new_obj)) {
                    has_new_obj = 1;
                }

            }

        }

        //如果已经没有引用在新生代了 则剔除该对象
        if(!has_new_obj){
            //obj->remembers = false;
            FL_UNSET(CURRENT_HEADER(rs[i]),FL_REMEMBERED);

            //剔除
            rs_index -- ;
            void *tmp = rs[rs_index];
            rs[rs_index] = rs[i];
            rs[i] = tmp;
        }
    }
}


/**
 * 写入屏障
 * 在更新对象引用的时候 需要判断obj是否是老年代对象  new_obj为年轻代对象
 * 如果是 则需要加入 记忆结果集
 * @param obj
 * @param field
 * @param new_obj
 */
void write_barrier(void *obj_ptr,void *field,void* new_obj_ptr)
{

    Header* obj     = CURRENT_HEADER(obj_ptr);
    Header* new_obj = CURRENT_HEADER(new_obj_ptr);
    //obj 在老年代
    //new_obj 在新生代
    //且 obj 未保存在 记忆集
    if(is_pointer_to_space(obj,oldg) &&
       !is_pointer_to_space(new_obj,oldg) &&
       !IS_REMEMBERED(obj))
    {
        rs[rs_index++] = obj;
        //设置该对象已经保存在了记忆集，无需再次保存
        FL_SET(obj,FL_REMEMBERED);
    }
    //obj->field = new_obj
    *(void **)field = new_obj_ptr;

}

/**
 * 新生代分配 不需要用到空闲链表 直接根据 free_p 来向后进行分配
 */
void*   minor_malloc(size_t req_size)
{
    DEBUG(printf("内存申请 :%ld\n",req_size));
    Header *obj;
    size_t do_gc = 0;
    req_size += HEADER_SIZE;
    //对齐 字节
    req_size = ALIGN(req_size, PTRSIZE);
    if (req_size <= 0) return NULL;

    alloc:
    //查看分配空间是否还够用
    if (new_free_p->size < req_size) {
        //一般是分块用尽会 才会执行gc 清除带回收的内存
        if (!do_gc) {
            do_gc = 1;
            //内存不够用的时候会触发 复制 释放空间
            //释放空间的时候会造成空间的压缩
            minor_gc();
            goto alloc;
        }
        printf("内存不够");
        return NULL;
    }
    obj = new_free_p;

    //将新生代空闲指针 移动到下一个空闲地址开头
    int left_size = new_free_p->size - req_size;
    new_free_p = (void*)(new_free_p) + req_size;
    //并设置正确的空余空间大小
    new_free_p->size = left_size;

    obj->size = req_size;
    obj->flags = 0;
    FL_SET(obj, FL_ALLOC);
    //设置年龄为0
    obj->age = 0;
    obj->forwarding = NULL;

    return obj+1;
}
void  minor_gc(void)
{
    printf("执行gc复制----\n");
    //每次gc前将 free指向 to的开头
    gc_heaps[survivortog].slot->size = gc_heaps[survivortog].size;
    //初始化to空间的首地址
    to_free_p = gc_heaps[survivortog].slot;

    //递归进行复制  从 from  => to
    for(int i = 0;i < root_used;i++){
        void* forwarded;
        if(!(forwarded = gc_copy(roots[i].ptr))) continue;

        *(Header**)roots[i].optr = forwarded;
        //将root 所有的执行换到 to上
        roots[i].ptr = forwarded;
    }

    //更新跨代引用
    update_reference();

    //清空 新生代
    new_free_p = gc_heaps[newg].slot ;
    memset(new_free_p,0,gc_heaps[newg].size);
    new_free_p->size = gc_heaps[newg].size;


    //清空 幸存代  from
    memset(gc_heaps[survivorfromg].slot,0,gc_heaps[survivorfromg].size);
    gc_heaps[survivorfromg].slot->size = gc_heaps[survivorfromg].size;

    //交换 swap(幸存代from ,幸存代to);
    GC_Heap tmp             = gc_heaps[survivorfromg];
    gc_heaps[survivorfromg] = gc_heaps[survivortog];
    gc_heaps[survivortog]   = tmp;
}

