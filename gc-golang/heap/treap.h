/**
 *@ClassName treap
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/29 0029 上午 11:12
 *@Version 1.0
 **/
#ifndef GOC_TREAP_H
#define GOC_TREAP_H

#include "../sys/gc.h"
#include "../span/span.h"

typedef struct mtreap     treap;
typedef struct mtreapNode treapNode;

/**
 * 红黑树，用于存储活跃的span页
 * 在本地缓存不够用的时候会向中心缓存获取
 */
struct mtreap{
    //执向头节点
    treapNode* root;
};

//heap堆结构
struct mtreapNode{

    treapNode* right;
    treapNode* left;
    treapNode* parent;

    size_t     npagesKey; // number of pages in spanKey, used as primary sort key
    span*      spankey;
    // 优先级 random number used by treap algorithm to keep tree probabilistically balanced treap 算法使用随机数来保持树的概率平衡
    uint32     priority;
};

treapNode* treap_find(treap* root,uintptr npages);
void treap_removeNode(treap* root,treapNode* t);
void treap_removeSpan(treap* root , span* s);
void treap_insert(treap* root,span* s);
treapNode* treap_end(treap* root);

treapNode* treap_pred(treapNode* t);
#endif //GOC_TREAP_H
