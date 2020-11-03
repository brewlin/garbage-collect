
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "refcount.h"
#include "gc.h"
void gc(void){
  //nothing need to do for referenceount
}
/**
 * 引用计数 + 1
 * @param ptr
 */
void gc_inc(void *ptr)
{
  //该内存属于哪个堆
  GC_Heap *gh;
  //该内存的header
  Header *hdr;

  //find header
  if (!(gh = is_pointer_to_heap(ptr))){
//      printf("not pointer\n");
    return;
  }
  if (!(hdr = get_header(gh, ptr))) {
    printf("not find header\n");
    return;
  }
  hdr->ref++;
}
/**
 * 引用计数- 1
 * 如果为0 则回收
 * 并对所有的引用进行 -1
 * @param ptr
 */
void gc_dec(void *ptr)
{
  //该内存属于哪个堆
  GC_Heap *gh;
  //该内存的header
  Header *hdr;
  //find header
  if (!(gh = is_pointer_to_heap(ptr))){
//    printf("not pointer\n");
    return;
  }
  if (!(hdr = get_header(gh, ptr))) {
    printf("not find header\n");
    return;
  }
  hdr->ref -- ;
  if (hdr->ref == 0) {
      //对引用对象进行递归标记
      void *p;
      void *end = (void*)NEXT_HEADER(hdr);
      //对引用进行递归 减引用
      for (p = ptr+1; p < end; p++) {
        //对内存解引用，因为内存里面可能存放了内存的地址 也就是引用，需要进行引用的递归标记
        gc_dec(*(void **)p);
      }
      //回收
      gc_free(ptr);
  }
}
/**
 * 获取当前的引用计数
 * @param ptr
 * @return
 */
int ref_count(void *ptr)
{
  //该内存属于哪个堆
  GC_Heap *gh;
  //该内存的header
  Header *hdr;
  //find header
  if (!(gh = is_pointer_to_heap(ptr))){
//    printf("not pointer\n");
    return NULL;
  }
  if (!(hdr = get_header(gh, ptr))) {
    printf("not find header\n");
    return NULL;
  }
  return hdr->ref;
}
/**
 * 指针赋值操作
 * @param ptr
 * @param obj
 */
void gc_update(void *ptr,void *obj)
{
  gc_inc(obj);
  gc_dec(*(void**)ptr);
  *(void**)ptr = obj;
}

