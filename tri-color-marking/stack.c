#include "stack.h"

void push(Stack* stk,void* v)
{
    Link* node  = malloc(sizeof(Link));
    node->value = v;
    node->next = NULL;

    if(stk->head == NULL)
        stk->head = node;
    if(stk->tail == NULL){
        stk->tail = stk->head;
    }else{
        stk->tail->next = node;
        stk->tail = node;
    }

}
int empty(Stack* stk){
    return stk->head == NULL;
}
void* pop(Stack* stk)
{
    if(stk->tail == NULL || stk->head == NULL)
        return NULL;

    Link* node = stk->head;
    stk->head = node->next;
    if(stk->head == NULL)
        stk->tail = NULL;

    void* v = node->value;
    free(node);
    return v;
}

