#include "../sys/gc.h"
#include "gc_work.h"
#include "../sys/gpm.h"
#include "root.h"

//线程tls 栈范围
__thread uintptr stk_start;
__thread uintptr stk_end;
//扫描根
//扫描栈

/**
 * 传入一个队列,里面所有对象都是灰色对象
 * @param gcw
 */
void gcDrain(gcWork* gcw)
{
    //初始化栈范围
    stk_end = get_sp();

    //高低往低地址增长
    uintptr cur_sp = stk_end;
    if(stk_start <= stk_end) throw("stack error!")
    for (; cur_sp < stk_start ; cur_sp += 4){
        span*   s;
        uintptr objIndex;
        uintptr base = findObject(*(void**)cur_sp,&s,&objIndex);
        //找到灰色对象
        if(base != 0){
            greyobject(base, s,gcw,objIndex);
        }
    }


    //开始标记剩余灰色对象
    //NOTICE: 目前不支持扫描对象
    while(true){
        uintptr obj = gcwork_tryGetFast(gcw);
        if(obj == NULL){
            obj = gcwork_tryGet(gcw);
            if(obj == NULL)
                break;
        }
    }

    //标记结束

}