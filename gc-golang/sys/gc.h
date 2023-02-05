/**
 *@ClassName gc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 上午 11:39
 *@Version 1.0
 **/
#ifndef GOC_GC_H
#define GOC_GC_H

#include <pthread.h>
#include "platform.h"
#include "defines.h"
#include <stdio.h>

typedef unsigned char      uint8;
typedef unsigned short int uint16;
typedef unsigned int       uint32;
typedef unsigned int       uint;
typedef unsigned long int  uint64;
typedef unsigned long int  uintptr;

typedef char               int8;
typedef short int          int16;
typedef int                int32;
typedef long  int          int64;
typedef long int           intptr;


typedef uint8              byte;
typedef uint8              bool;


/**
 * thread mutex
 */

typedef pthread_mutex_t    mutex;

extern  byte                deBruijnIdx64[64];
uintptr round(uintptr n,uintptr a);
uint32 fastrand();
int ctz64(uint64 x);

typedef struct
{
    uintptr size;
    // size of memory prefix holding all pointers
    uintptr ptrdata;
    //对象类型，有需要包含指针的 有不需要包含指针的
    //对于gc来说特别重要，这意味着可能需要递归扫描指针引用
    uint8   kind;
}type;

//gc
//gc处于标记阶段的时候会 置于1
extern uint32 gcBlackenEnabled;
//执行当前gc所处的阶段
//修改时需要保证原子性
extern uint32 gcphase;
extern uint8 oneBitCount[256];
extern int32  ncpu;

#define DEBUG(fmt, ...) \
  printf(fmt, ##__VA_ARGS__);   \
  printf(" %s() %s:%d\n", __func__, __FILE__, __LINE__)

#endif //GOC_GC_H
