//
// Created by root on 2021/4/17.
//

#ifndef GOC_PROC_H
#define GOC_PROC_H

#include <pthread.h>
#include "../sys/gpm.h"

void stopTheWorld(m** allm , int ms);
void startTheWorld();
void regsig();
void sighandler(int signo);


extern __thread int mid;

#endif //GOC_PROC_H
