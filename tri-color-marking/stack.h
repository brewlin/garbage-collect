/**
 *@ClassName stack
 *@Deacription go
 *@Author brewlin
 *@Date 2020/10/30 0030 上午 10:08
 *@Version 1.0
 **/
#ifndef GC_LEARNING_STACK_H
#define GC_LEARNING_STACK_H

#include <malloc.h>

typedef struct link_list
{
    void*             value;
    struct link_list *next;
}Link;

typedef struct stack_header
{
    Link* head;
    Link* tail;
}Stack;

void push(Stack* stk,void* v);
int empty(Stack* stk);
void* pop(Stack* stk);

#endif //GC_LEARNING_STACK_H
