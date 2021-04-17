/**
 *@Deacription go
 *@Author brewlin
 *@Date 2021/4/1 0001 下午 3:39
 *@Version 1.0
 **/
#include "../sys/gc.h"
#include "../sys/gpm.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/signal.h>
#include "../atomic/atomic.h"
__thread int mid;

//全局条件变量
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
void sighandler(int signo);
/**
 *
 */
void regsig()
{
    struct sigaction actions;
    sigemptyset(&actions.sa_mask);
    /* 将参数set信号集初始化并清空 */
    actions.sa_handler = sighandler;
    actions.sa_flags = -1;
    sigaction(SIGURG,&actions,NULL);
}
/**
 *
 * @param signo
 */
void sighandler(int signo)
{
    //不可靠信号需要重新安装
    regsig();
    //等待主线程释放锁
    allm[mid]->mallocing ++;
    pthread_mutex_lock(&mtx);
    pthread_mutex_unlock(&mtx);
    allm[mid]->mallocing --;
    return;
}
/**
 *
 * @param allm
 * @param ms
 */
void stopTheWorld(m** allm , int ms)
{
    pthread_mutex_lock(&mtx);
    //这里发送信号其他所有线程
    for(int i = 0 ; i < ms ; i ++){
        if(allm[i] != NULL && allm[i]->mid != mid && allm[i]->mallocing == 0){
            pthread_kill(allm[i]->pid,SIGURG);
        }
    }
}
/**
 *
 */
void startTheWorld()
{
    pthread_mutex_unlock(&mtx);
}
