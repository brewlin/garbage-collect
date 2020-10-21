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
 * This function decrements the integer, locking the object when doing so
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
void main(){
  test_multi_gc();
  //测试对象引用
  test_reference();

}
