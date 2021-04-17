/**
 *@ClassName gpm
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/30 0030 下午 3:42
 *@Version 1.0
 **/
#include "gc.h"
#include "gpm.h"


__thread  g* _g_;
/**
 * 多线程安全获取 当前线程变量m
 * @return
 */
m* acquirem()
{

    return getg()->m;
}
void releasem(m* m){

}
/**
 * @return
 */
g* getg()
{
    return _g_;
}
