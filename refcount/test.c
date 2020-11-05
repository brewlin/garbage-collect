#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "refcount.h"
#include "gc.h"

#define ITERS 100000

typedef struct {
    pthread_mutex_t mutex;
    int value;
} Data;
/**
 * 对value进行递增
 * 对于外来变量 在函数前都应该 inc 计数
 * 结束后 dec计数
 * @param arg
 * @return
 */
void *incr(void *arg)
{
  gc_inc(arg);
  Data* data = (Data*)arg;

  printf("\tincr:引用计数: %d\n", ref_count(data));
  for (int i = 0; i < ITERS; ++i)
  {
    pthread_mutex_lock(&data->mutex);
    data->value++;
    pthread_mutex_unlock(&data->mutex);
    sched_yield();
  }
  gc_dec(data);
  printf("\tincr:回收前的引用计数: %d\n", ref_count(data));
  return 0;
}
/**
 * 对 value 进行递减
 * 对于外来变量 在函数前都应该 inc 计数
 * 结束后 dec计数
 * @param arg
 * @return
 */
void *decr(void *arg)
{
  gc_inc(arg);
  Data* data = (Data*)arg;

  printf("\tdecr: 引用计数: %d\n", ref_count(data));
  for (int i = ITERS - 1; i >= 0; --i)
  {
    pthread_mutex_lock(&data->mutex);
    data->value--;
    pthread_mutex_unlock(&data->mutex);

    //让出cpu
    sched_yield();
  }
  gc_dec(data);
  printf("\tdecr:回收前的引用计数: %d\n", ref_count(data));
  return 0;
}
/**
 * 测试多线程gc
 * @return
 */
int test_multi_gc(void)
{

  pthread_t incr_thr;
  pthread_t decr_thr;

  Data* data = gc_malloc(sizeof(Data));
  if (data == NULL) exit(1);

  printf("gc malloc ref count: %d\n", ref_count(data));

  data->value = 100;
  printf("start value: %d\n", data->value);

  /* create incrementing thread */
  if (pthread_create(&incr_thr, NULL, &incr, data)) {
    gc_dec(data);
    fprintf(stderr, "Could not create thread\n");
    exit(1);
  }

  /* create decrementing thread */
  if (pthread_create(&decr_thr, NULL, &decr, data)) {
    gc_dec(data);
    fprintf(stderr, "Could not create thread\n");
    exit(1);
  }

  /* wait for threads to finish */
  if (pthread_join(incr_thr, NULL)) {
    fprintf(stderr, "Could not join\n");
    exit(1);
  }
  if (pthread_join(decr_thr, NULL)) {
    fprintf(stderr, "Could not join\n");
    exit(1);
  }

  /* count should be the same as the original */
  printf("end value: %d\n", data->value);
  //如果引用技术管理错误 则value不会是100
  assert(data->value == 100);
  printf("回收前的引用计数: %d\n", ref_count(data));
  //回收
  gc_dec(data);
  assert(data->value == 0);


  return 0;
}
/**
 * 将多个对象关联起来，只dec其中一个对象 看看是否会被回收
 * 实际应该所有的都会被回收
 */
void test_reference(){
  typedef struct t{
      int v;
      struct t* left;
      struct t* right;
  }T;
  T* t = gc_malloc(sizeof(T));
  t->right = gc_malloc(sizeof(T));
  t->left = gc_malloc(sizeof(T));

  T* r = t->right;
  T* l = t->left;
  r->v = 100;
  l->v = 200;
  gc_dec(t);
  //因为对t进行 -1时
  //会递归去寻找相关引用，且 left和right本身只有 1，所以会被回收
  assert(r->v == 0);
  assert(l->v == 0);
}
void test_reference2(){
  typedef struct t{
      int v;
      struct t* left;
      struct t* right;
  }T;
  T* t = gc_malloc(sizeof(T));
  t->right = gc_malloc(sizeof(T));
  t->left = gc_malloc(sizeof(T));

  //现在 right的引用计数为 2
  T* r = t->right;
  T* l = t->left;
  r->v = 100;
  l->v = 200;
  //对r进行 ref += 1
  gc_inc(r);

  //对t进行 ref -= 1
  gc_dec(t);
  //因为对t进行 -1时 right引用计数-1 = 1 不会被回收
  assert(r->v == 100);
  //会递归去寻找相关引用，且 left本身只有 1，所以会被回收
  assert(l->v == 0);
}
/**
 * 测试更新指针的测试
 */
void  test_update_ptr(){

  typedef struct t {
      int v;
      struct t *ptr;
  }T;
  T* a = gc_malloc(sizeof(T));
  T* b = gc_malloc(sizeof(T));
  T* c = gc_malloc(sizeof(T));

  //现在 a_ref=1  b_ref = 2;
  gc_update(&a->ptr,b);
  assert(ref_count(b) == 2);

  //现在 a_ref=1  b_ref = 1 c _ref = 2;
  gc_update(&a->ptr,c);
  assert(ref_count(c) == 2);
  assert(ref_count(b) == 1);

  //a_ref = 0 b_ref = 1 c_ref = 1;
  gc_dec(a);
  assert(ref_count(a) == 0);
  assert(ref_count(b) == 1);
  assert(ref_count(c) == 1);


}
int main(){
  printf("------------测试多线程引用计数---------------\n");
  test_multi_gc();
  printf("-----------    passing    ---------------\n\n");


  printf("------------测试多引用计数1  ---------------\n");
  //测试对象引用
  test_reference();
  printf("-----------    passing    ---------------\n\n");

  printf("------------测试多引用计数2  ---------------\n");
  //测试对象引用
  test_reference2();
  printf("-----------    passing    ---------------\n\n");


  printf("------------测试指针更新计数  ---------------\n");
  //测试对象引用
  test_update_ptr();
  printf("-----------    passing    ---------------\n\n");
  return 0;
}
