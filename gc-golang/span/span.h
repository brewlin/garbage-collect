/**
 *@ClassName span
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 上午 11:27
 *@Version 1.0
 **/
#ifndef GOC_SPAN_H
#define GOC_SPAN_H
#include "../sys/gc.h"
typedef struct mspan     span;
typedef struct mspanlist spanlist;
typedef uint8  spanclass;

// 对应golang的 type mspan struct
struct mspan{
    //双向链表
    span* next;
    span* prev;
    //关联了表头，方便debug
    spanlist* list;

    uintptr  startaddr;
    uintptr  limit; //指向span已使用空间的末尾
    uintptr  npages;//记录span里的页数量 ,一个span里有多少页


    uint8    state; //当前span的状态
    uint8    needzero;//是否需要申请前格式化为0
    uint8    scavenged; // whether this span has had its pages released to the OS
    uint16   allocCount; //记录了申请了多少对象
    // sweep generation: h->sweepgen 是全局heap上的
    // if sweepgen == h->sweepgen - 2, the span needs sweeping  如果当前sweepgen 等于 全局sweepgen - 2 这个span需要被清除
    // if sweepgen == h->sweepgen - 1, the span is currently being swept 正在回收
    // if sweepgen == h->sweepgen, the span is swept and ready to use    已经被回收完了 可以使用
    // if sweepgen == h->sweepgen + 1, the span was cached before sweep began and is still cached, and needs sweeping
    // if sweepgen == h->sweepgen + 3, the span was swept and then cached and is still cached
    // h->sweepgen is incremented by 2 after every GC
    uint32    sweepgen;

    int64     unusedsince;// first time spotted by gc in Mspanfree state
    uint8     spanclass;  // size class and noscan (uint8)

    uint16    basemask;   // if non-0, elemsize is a power of 2, & this will get object allocation base
    uint8     divshift;   // for divide by elemsize - divMagic.shift
    uint8     divshift2;  // for divide by elemsize - divMagic.shift2
    uint16    divmul;     // for divide by elemsize - divMagic.mul

    uintptr   elemsize;   // 当前span总共可以分配多少个对象 computed from sizeclass or from npages

    //位图和gc相关
    //freeindex是一个标志位，我们知道span里的对象大小都是一样的，所以freeindex指向空闲对象地址的标志位
    //如果freeindex == nelem 等于空 则空间满了，sapn不能继续分配了
    uintptr   freeindex;
    uintptr   nelems;
    //allocBits 是一个位图对象
    //如果 n >= freeindex and allocBits[n/8] & (1<<(n%8)) == 0
    //说明对象n是空闲可用于分配的内存
    //所以对象n从 n*elemsize + (start << pageShift) 地址处开始分配
    /*
    allocBits 和 gcmarkBits 持有指向 span 的标记和分配位的指针。 指针是 8 字节对齐的。 这些数据保存在三个arenas区域。 
    free：不再访问的肮脏arenas并且可以重复使用。
    next：保存下一个GC周期要用到的信息。 
    当前：此 GC 周期中正在使用的信息。 
    previous：在上一个 GC 周期中使用的信息。 新的 GC 周期从调用 finishsweep_m 开始。 finishsweep_m 将上一个竞技场移动到空闲竞技场，将当前竞技场移动到上一个竞技场，将下一个竞技场移动到当前竞技场。 下一个 arena 被填充，因为跨度请求内存以保存下一个 GC 周期的 gcmarkBits 以及新分配的跨度的 allocBits。
    指针算法是“手动”完成的，而不是使用数组来避免沿关键性能路径进行边界检查。 扫描将释放旧的 allocBits 并将 allocBits 设置为 gcmarkBits。 gcmarkBits 被新清零的内存所取代
    */
    uint8*    allocBits;
    uint8*    gcmarkBits;
    //8字节 64位来标记每个 每个slot是否被使用的情况

    // Cache of the allocBits at freeindex. allocCache is shifted such that the lowest bit corresponds to the bit freeindex. allocCache holds the complement of allocBits, thus allowing ctz (count trailing zero) to use it directly. allocCache may contain bits beyond s.nelems; the caller must ignore these. freeindex 处的 allocBits 缓存。 allocCache 被移动，使得最低位对应于 freeindex 位。 allocCache 持有 allocBits 的补码，因此允许 ctz（计数尾随零）直接使用它。 allocCache 可能包含超出 s.nelems 的位； 调用者必须忽略这些。
    uint64    allocCache;
};

struct mspanlist {
    span* first;
    span* last;
};

//span
void    span_init(span* s,uintptr base,uintptr npages);
void    span_ppbounds(span* s, uintptr* start,uintptr* end);
uintptr span_released(span* s);
uintptr span_scavenge(span* s);
void    span_refillAllocCache(span* s,uintptr whichByte);
uintptr span_nextFreeIndex(span* s);
int     span_countAlloc(span* s);
void    span_layout(span* s ,uintptr* size,uintptr* n,uintptr* total);
bool    span_isFree(span* s ,uintptr  index);

//spanlist
void    spanlist_remove(spanlist* list,span* s);
void    spanlist_insertback(spanlist* list,span* s);
void    spanlist_insert(spanlist* list,span* s);

int8      sizeclass(spanclass sc);
bool      noscan(spanclass sc);
spanclass makeSpanClass(spanclass sc,bool noscan);


typedef struct {
    uint8   shift;
    uint8   shift2;
    uint8   mul;
    uint8   baseMask;
}divMagic;

extern uint16   class_to_size[_NumSizeClasses];
extern uint8    class_to_allocnpages[_NumSizeClasses];

extern divMagic class_to_divmagic[_NumSizeClasses];
extern uint8    size_to_class8[smallSizeMax/smallSizeDiv + 1];
extern uint8    size_to_class128[(_MaxSmallSize-smallSizeMax)/largeSizeDiv + 1];
extern uint16   class_to_size[_NumSizeClasses];
extern uint8    class_to_allocnpages[_NumSizeClasses];

#endif //GOC_SPAN_H
