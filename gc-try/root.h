/**
 *@ClassName root
 *@Deacription go
 *@Author brewlin
 *@Date 2020/11/6 0006 下午 3:18
 *@Version 1.0
 **/
#ifndef GC_LEARNING_ROOT_H
#define GC_LEARNING_ROOT_H

void* get_sp()asm("get_sp");
void* get_bp()asm("get_bp");
void* get_di()asm("get_di");
void* get_si()asm("get_si");
void* get_dx()asm("get_dx");
void* get_cx()asm("get_cx");
void* get_r8()asm("get_r8");
void* get_r9()asm("get_r9");
void* get_bp()asm("get_bp");
void* get_ax()asm("get_ax");
void* get_bx()asm("get_bx");

#endif //GC_LEARNING_ROOT_H
