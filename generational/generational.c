#include "gc.h"
#include "generational.h"

void* rs[ROOT_RANGES_LIMIT];
int   rs_index = 0;

//保存了所有申请的对象
root roots[ROOT_RANGES_LIMIT];
size_t root_used = 0;
//每次复制前将 该指针 指向 to的首地址
void* to_free_p;

/**
 * 开始进行gc标记
 **/
void* gc_mark(void *ptr){
    Header *hdr;
    if (!(hdr = get_header(ptr))) {
//      printf("not find header\n");
      return ptr;
    }
    if (!FL_TEST(hdr, FL_ALLOC)) {
      printf("flag not set alloc\n");
      return ptr;
    }
    if (FL_TEST(hdr, FL_MARK)) {
      //printf("flag not set mark\n");
      return ptr;
    }

    /* marking */
    FL_SET(hdr, FL_MARK);
//    printf("mark ptr : %p, header : %p\n", ptr, hdr);
    //进行子节点递归 标记
    gc_mark_or_copy_range((void *)(hdr+1) + 1, (void *)NEXT_HEADER(hdr));
    return ptr;
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
    void* new_obj = gc_old_malloc(CURRENT_HEADER(ptr)->size + HEADER_SIZE);
    if(new_obj == NULL){
        //执行老年代GC ，也就是标记清除算法
        major_gc();
        new_obj = gc_old_malloc(CURRENT_HEADER(ptr)->size + HEADER_SIZE);
        if(new_obj == NULL) abort();
    }
    //将obj 拷贝到 new_obj中
    memcpy(new_obj,ptr,obj->size);


    obj->forwarding = new_obj;
    //标志已经复制过了 forwarded = true
    FL_SET(obj,FL_COPIED);

    //for child: obj
    for (void *p = ptr; p < ptr + obj->size + ; p++) {

        Header* hdr;
        //解引用 如果该内存依然是指向的from，且有forwarding 则需要改了
        void *ptr = *(void**)p;
        //查看该引用是否存在于 新生代
        if (!(hdr = get_header(ptr))) {
            continue;
        }
        //存在就要将 new_obj 加入集合
        rs[rs_index++] = new_obj;
        break;
    }





}
/**
 * 进行子对象拷贝
 */
void* gc_copy(void *ptr){
    Header *hdr;

    if (!(hdr = get_header(ptr))) {
//      printf("not find header\n");
      return NULL;
    }
    assert(FL_TEST(hdr,FL_ALLOC));
    //TODO: to空间不够了之后 可以将这些数据复制到 老年代空间去
    if((hdr->size + HEADER_SIZE) > ((Header*)to_free_p)->size){
        printf("to 空间不够了 gc 复制阶段错误\n");
        abort();
    }
    //没有复制过  0
    if(!IS_COPIED(hdr)){
        //判断年龄是否小于阈值
        if(hdr->age < AGE_MAX)
        {
            //计算复制后的指针
            Header *forwarding = (Header*)to_free_p;
            //在准备分配前的总空间
            size_t total = forwarding->size;
            //分配一份内存 将源对象拷贝过来
            memcpy(forwarding, hdr, hdr->size+HEADER_SIZE);
            //拷贝过后 将原对象标记为已拷贝 为后面递归引用复制做准备
            FL_SET(hdr,FL_COPIED);
            //更新拷贝过后新对象的年龄
            forwarding->age ++;

            //free 指向下一个 body
            to_free_p += (HEADER_SIZE + hdr->size);
            //free_p 执行的剩余空间需要时刻维护着
            ((Header*)to_free_p)->size = total - (hdr->size + HEADER_SIZE);
            //源对象 保留 新对象的引用
            hdr->forwarding = forwarding;

            printf("需要执行拷贝 ptr:%p hdr:%p  after:%p\n",ptr,hdr,forwarding);
            //从forwarding 指向的空间开始递归
            gc_mark_or_copy_range((void *)(forwarding+1) + 1, (void *)NEXT_HEADER(forwarding));
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
 * 遍历root 进行标记
 * @param start
 * @param end
 */
void*  gc_copy_range(void *start, void *end)
{
    void *p;

    void* new_obj = gc_copy(start);
    //可能申请的内存 里面又包含了其他内存
    for (p = start + 1; p < end; p++) {
         //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
         //递归进行 引用的拷贝
         gc_copy(*(void **)p);
    }
    //返回 body
    return new_obj;
}
/**
 * 将分配的变量 添加到root 引用,只要是root上的对象都能够进行标记
 * @param start
 * @param end
 */
void     add_roots(void* o_ptr)
{
    void *ptr = *(void**)o_ptr;

    Header* hdr = (Header*)ptr - 1;
    roots[root_used].start = ptr;
    roots[root_used].end = ptr + hdr->size;
    roots[root_used].optr = o_ptr;
    root_used++;

    if (root_used >= ROOT_RANGES_LIMIT) {
        fputs("Root OverFlow", stderr);
        abort();
    }
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
        void* end   =  start + CURRENT_HEADER(rs[i])->size;

        //判断对该对象递归引用拷贝后，是否还有继续在新生代的引用
        int has_new_obj = 0;
        //可能申请的内存 里面又包含了其他内存
        for (void *p = start; p < end; p++) {

            Header* hdr;
            void *ptr = *(void**)p;
            //如果 child 引用属于新生代 则需要 进行拷贝
            if (!(hdr = get_header(ptr))) {

                void* new_obj = gc_copy(ptr);
                printf("拷贝引用 hdr:%p new_obj:%p\n",hdr, new_obj);
                *(Header**)p = new_obj;

                //判断拷贝过后的 new_obj 是否还是继续属于新生代
                if (!(hdr = get_header(new_obj))) {
                    has_new_obj = 1;
                }

            }

        }

        //如果已经没有引用在新生代了 则剔除该对象
        if(!has_new_obj){
            //obj->remembers = false;
            FL_UNSET(CURRENT_HEADER(rs[i]),FL_REMEMBERED)

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
    if(obj > $old_start && new_obj < $old_start && !IS_REMEMBERED($obj)){
        rs[rs_index++] = obj;
        //设置该对象已经保存在了记忆集，无需再次保存
        FL_SET(obj,FL_REMEMBERED);
    }
    //obj->field = new_obj
    *(void **)field = new_obj_ptr;

}
/**
 * 查看该对象是否存在于from堆上
 */
int is_pointer_to_old_space(void* ptr)
{

    void* from_start = gc_heaps[oldg].slot;
    void* from_end   = (void*)gc_heaps[oldg].slot + HEADER_SIZE + gc_heaps[oldg].size;
    if((void*)ptr <= from_end && (void*)ptr >= from_start)
        return 1;
    return 0;
}
void  gc(void)
{
    printf("执行gc复制----\n");
    //每次gc前将 free指向 to的开头
    gc_heaps[to].slot->size = gc_heaps[to].size;
    //初始化to空间的首地址
    to_free_p = gc_heaps[survivortog].slot;

    //递归进行复制  从 from  => to
    for(int i = 0;i < root_used;i++){
        void* forwarded = gc_copy_range(roots[i].start, roots[i].end);
        //判断 新对象 是否属于老年代堆
        if(!is_pointer_to_old_space(forwarded)){
            *(Header**)roots[i].optr = forwarded;
            //将root 所有的执行换到 to上
            roots[i].start = forwarded;
            roots[i].end   = forwarded + CURRENT_HEADER(forwarded)->size;
        }
        //晋升为老年代的对象不用 更新
    }

    //更新跨代引用
    update_reference();

    //清空 新生代
    gc_free(gc_heaps[newg].slot + 1);
    new_free_p = gc_heaps[newg].slot ;
    new_free_p->size = gc_heaps[newg].size;

    //清空 幸存代  from
    gc_free(gc_heaps[survivorfromg].slot + 1);

    //交换 swap(幸存代from ,幸存代to);
    GC_Heap tmp             = gc_heaps[survivorfromg];
    gc_heaps[survivorfromg] = gc_heaps[survivortog];
    gc_heaps[survivortog]   = tmp;
}

