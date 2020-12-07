
#ifndef GC_LIST_H
#define GC_LIST_H

#include <stdio.h>
#include <stdlib.h>
typedef struct link_list
{
    void*             value;
    int               size;
    struct link_list *next;
}ListNode;

typedef struct list{
    ListNode *root;
}List;
void  push(List* list,void* v,int size);
void  mark(List* list,void* v);
void  del(List* list,void* v);

#endif //GC_LIST_H
