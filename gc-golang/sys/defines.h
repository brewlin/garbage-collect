//
// Created by root on 2021/4/5.
//

#ifndef GOC_DEFINES_H
#define GOC_DEFINES_H
//common
#define true               1
#define false              0
#define ERROR              -1
#define OK                 0
//span
#define _MaxSmallSize   32768
#define smallSizeDiv    8
#define smallSizeMax    1024
#define largeSizeDiv    128
#define _NumSizeClasses 67 // 共有67种内存尺寸, size class 从 1 到 66 共 66 个，代码中_NumSizeClasses=67代表了实际使用的 size class 数量，即 67 个，从 0 到 67，size class 0 实际并未使用到
#define _PageShift      13 // 每页大小为8k
#define numSpanClasses  (_NumSizeClasses << 1)
#define tinySpanClass   (uint8)(tinySizeClass<<1 | 1)

#define  mSpanDead   1
#define  mSpanInUse  2           // allocated for garbage collected heap
#define  mSpanManual 3           // allocated for manual management (e.g., stack allocator)
#define  mSpanFree   4

#define lock               pthread_mutex_lock
#define unlock             pthread_mutex_unlock
#define mutex_init(lock)         pthread_mutex_init(lock,NULL)


// marco
#define pageShift           13
#define pageSize            (1 << pageShift)
#define _PageSize           (1 << pageShift)
#define PageMask            (pageSize - 1)
#define persistentChunkSize (256 << 10)
#define ptrSize             (4 << (~(uintptr)0 >> 63))
#define throw(str)          {printf(str);int _a = 10/0;}
#define arenaBaseOffset     ((uintptr)(1 << 47))
#define _64bit              (1 << (~(uintptr)0 >> 63) / 2)
#define logHeapArenaBytes   ((6+20)*(_64bit)  + (2+20)*(1-_64bit))
#define heapArenaBytes      (1 << logHeapArenaBytes)
#define uintptrMask         (1<<(8*ptrSize) - 1)

#define heapAddrBits        ((_64bit*(1-GoarchWasm)*(1-GoosAix))*48 + (1-_64bit+GoarchWasm)*(32-(GoarchMips+GoarchMipsle)) + 60*GoosAix)
#define arenaL1Bits         (6*(_64bit*GoosWindows) + 12*GoosAix)
#define arenaL2Bits         (heapAddrBits - logHeapArenaBytes - arenaL1Bits)
#define arenaBits           ( arenaL1Bits + arenaL2Bits)
#define arenaL1Shift        arenaL2Bits
#define pagesPerArena       (heapArenaBytes / pageSize)
#define heapArenaBitmapBytes (heapArenaBytes / (ptrSize * 8 / 2))

#define _TinySize           16
#define _TinySizeClass      ((int8)2)
#define maxTinySize         _TinySize
#define tinySizeClass       _TinySizeClass
#define _MaxSmallSize       32768
#define maxSmallSize        _MaxSmallSize
#define maxTinySize         _TinySize

#define wordsPerBitmapByte  (8 / 2) // heap words described by one bitmap byte
#define heapBitsShift       1     // shift offset between successive bitPointer or bitScan entries
#define bitPointer          (1 << 0)
#define bitScan             (1 << 4)
// all scan/pointer bits in a byte
#define bitScanAll          (bitScan | bitScan<<heapBitsShift | bitScan<<(2*heapBitsShift) | bitScan<<(3*heapBitsShift))
#define bitPointerAll       (bitPointer | bitPointer<<heapBitsShift | bitPointer<<(2*heapBitsShift) | bitPointer<<(3*heapBitsShift))


//alloc
#define fixAllocChunk (16 << 10)

#define deBruijn64          0x0218a392cd3d5dbf

//gc
#define     _GCoff         1 // GC not running; sweeping in background, write barrier disabled //gc没有运行 后台在清除，写屏障关闭
#define     _GCmark        2                  // GC marking roots and workbufs: allocate black, write barrier EjNABLED //gc标记根和buff，
#define     _GCmarktermination 3        // GC mark termination: allocate black, P's help GC, write barrier ENABLED//gc标记结束，

//array
#define ARRAY_SIZE 8
#define ARRAY_GET(arr,i) (arr->addr + arr->size*i)

#endif //GOC_DEFINES_H
