#include "Hugmem.h"
#include "gc.h"


void push(List* list,void* v,int size)
{
    ListNode* node = malloc(sizeof(ListNode));
    node->value = v;
    node->size = size;
    node->next = NULL;
    if(list->root == NULL){
        list->root = node;
        return;
    }
    node->next = list->root;
    list->root = node;
}
void  mark(List* list,void* v){
    ListNode* cur = list->root;
    while(cur != NULL){
        if(cur->value == v){
            Header* hdr = (Header*)((void*)v - 8);
            FL_SET(hdr->flags,FL_MARK);
            for (void *p  = v; p < (v + cur->size -8); p += 1){
                gc_mark(*(void**)p);
            }
            
            return;
        }
        cur = cur->next;
    }
}
void del(List* list,void *v)
{
    if(!list->root || !v)return;

    ListNode* tmp = list->root;
    if(tmp->value == v){
        list->root = tmp->next;
        return;
    }
    while(tmp != NULL && tmp->next != NULL){
        if(tmp->next->value == v){
            tmp->next = tmp->next->next;
            return;
        }
        tmp = tmp->next;
    }

}

