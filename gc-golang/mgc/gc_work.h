#ifndef GOC_GC_WORK_H
#define GOC_GC_WORK_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct _gcWork gcWork;

typedef struct {
    uint64  next;
    uintptr pushcnt;
}lfnode;

#define _WorkbufSize 2048
#define _workbufObjsize ((_WorkbufSize-24) / ptrSize)
typedef struct {
    lfnode     node;
    int        nobj;
    // account for the above fields
    uintptr    obj[_workbufObjsize];
}workbuf;

struct _gcWork {
    workbuf *wbuf1,*wbuf2;

    //统计当前工作缓冲区已经标记为黑色的字节数量
    uint64   bytesMarked;
    int64    scanWork;
    bool     flushedWork;
    uint32   pauseGen;
    uint32   putGen;
    uintptr  pauseStack[16];
};

typedef struct {
    mutex  locks;//TODO: init
    spanlist free;
    spanlist busy;
}workerSpans;

typedef struct {
    uint64 full;
    uint64 empty;

    //是否是强制型GC
    bool   userForced;
    //gc执行了几轮
    uint32 cycles;
    workerSpans wbufSpans;
    uint32 gcing;
}_work;

extern _work work;

bool gcwork_putFast(gcWork* w,uintptr obj);
void gcwork_put(gcWork* w, uintptr obj);
#endif //GOC_GC_WORK_H
