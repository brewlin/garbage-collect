
/**
 *@ClassName treap
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/30 0030 下午 1:50
 *@Version 1->0
 **/

#include "treap.h"
#include "heap.h"
#include "../malloc/fixalloc.h"

/**
 *
 * @param root
 * @param x
 */
static void rotateLeft(treap* root,treapNode* x)
{
	// p -> (x a (y b c))
	treapNode* p = x->parent;
	treapNode* a,*y,*b,*c;
	a = x->left;
	y = x->right;
	b = y->left;
	c = y->right;

	y->left = x;
	x->parent = y;
	y->right = c;
	if ( c != NULL ){
		c->parent = y;
	}
	x->left = a;
	if ( a != NULL ){
		a->parent = x;
	}
	x->right = b;
	if ( b != NULL ){
		b->parent = x;
	}

	y->parent = p;
	if ( p == NULL ){
		root->root = y;
	} else if ( p->left == x ){
		p->left = y;
	} else {
		if ( p->right != x ){
			printf("large span treap rotateLeft");
			exit(-1);
		}
		p->right = y;
	}
}

/**
 * 
 * @param y 
 */
static void rotateRight(treap* root,treapNode* y)
{

	// p -> (y (x a b) c)
	treapNode* p = y->parent,*x,*c,*a,*b;
	x = y->left;
	c = y->right;
	a = x->left;
	b = x->right;

	x->left = a;
	if ( a != NULL ){
		a->parent = x;
	}
	x->right = y;
	y->parent = x;
	y->left = b;
	if ( b != NULL ){
		b->parent = y;
	}
	y->right = c;
	if ( c != NULL ){
		c->parent = y;
	}

	x->parent = p;
	if ( p == NULL ){
		root->root = x;
	} else if ( p->left == y ){
		p->left = x;
	} else {
		if ( p->right != y ){
		    throw("large span treap rotateRight")
		}
		p->right = x;
	}
}


/**
 * 遍历树结构找到一个符合的页并返回
 * @param root
 * @param npages
 * @return
 */
treapNode* treap_find(treap* root,uintptr npages){
	treapNode* best = NULL;
	treapNode* t = root->root;
	while(t != NULL)
	{
		if(t->spankey == NULL){
			printf("treap node with NULL SpanKey found");
			exit(-1);
		}
		if( t->npagesKey >= npages ){
			best = t;
			t = t->left;
		} else {
			t = t->right;
		}
	}
	return best;
}

/**
 *
 * @param root
 * @param t
 */
void treap_removeNode(treap* root,treapNode* t){

	if(t->spankey->npages != t->npagesKey ){
		throw("span and treap node npages do not match\n");
	}
	// 树节点的删除
	while(t->right != NULL || t->left != NULL ) {
		if ( t->right == NULL || t->left != NULL &&  t->left->priority < t->right->priority ) {
			rotateRight(root,t);
		} else {
			rotateLeft(root,t);
		}
	}
	// Remove t, now a leaf->
	if ( t->parent != NULL ) {
		if (t->parent->left == t ){
			t->parent->left = NULL;
		} else {
			t->parent->right = NULL;
		}
	} else {
		root->root = NULL;
	}
	// 回收node，下次继续使用
	fixalloc_free(&heap_.treapalloc,t);
}

// removeSpan searches for, finds, deletes span along with
// the associated treap node-> If the span is not in the treap
// then t will eventually be set to NULL and the t->spanKey
// will throw->
/**
 * @param root
 * @param s
 */
void treap_removeSpan(treap* root , span* s)
{
	uintptr npages = s->npages;
	treapNode* t = root->root;
	while( t->spankey != s ){
		if ( t->npagesKey < npages ) {
			t = t->right;
		} else if ( t->npagesKey > npages ) {
			t = t->left;
		} else if ( t->spankey->startaddr < s->startaddr ) {
			t = t->right;
		} else if ( t->spankey->startaddr > s->startaddr ) {
			t = t->left;
		}
	}
	treap_removeNode(root,t);
}

/**
 *
 * insert adds span to the large span treap->
 * @param root
 * @param s
 */
void treap_insert(treap* root,span* s)
{

	uintptr  npages = s->npages;
	treapNode* last;
	treapNode** pt = &root->root;
	for ( treapNode* t = *pt; t != NULL; t = *pt ) {
		last = t;
		if ( t->npagesKey < npages ){
			pt = &t->right;
		} else if ( t->npagesKey > npages ){
			pt = &t->left;
		} else if ( t->spankey->startaddr < s->startaddr ){
			// t->npagesKey == npages, so sort on span addresses->
			pt = &t->right;
		} else if ( t->spankey->startaddr > s->startaddr ){
			pt = &t->left;
		} else {
			throw("inserting span already in treap\n");
		}
	}

	//分配一个treap node内存
	treapNode* t = (treapNode*)fixalloc_alloc(&heap_.treapalloc);
	t->npagesKey = s->npages;
	t->priority  = fastrand();
	t->spankey   = s;
	t->parent    = last;
	*pt           = t; // t now at a leaf->
	// Rotate up into tree according to priority->
	while( t->parent != NULL && t->parent->priority > t->priority ) {
		if ( t != NULL && t->spankey->npages != t->npagesKey ) {
			printf("runtime: insert t=%p t->npagesKey=%ld\n",t, t->npagesKey);
			printf("runtime:      t->spankey=%ld t->spankey->npages=%ld\n",t->spankey, t->spankey->npages);
			printf("span and treap sizes do not match?");
			exit(-1);
		}
		if ( t->parent->left == t ) {
			rotateRight(root,t->parent);
		} else {
			if (t->parent->right != t ) {
				printf("treap insert finds a broken treap");
				exit(-1);
			}
			rotateLeft(root,t->parent);
		}
	}
}

/**
 *
 * @param root
 * @return
 */
treapNode* treap_end(treap* root)
{
    treapNode* t = root->root;
    if(t == NULL) return NULL;
    while(t->right != NULL)
        t = t->right;
    return t;
}

/**
 *
 * @param t
 * @return
 */
treapNode* treap_pred(treapNode* t)
{
    if ( t->left != NULL ) {
        // If it has a left child, its predecessor will be
        // its right most left (grand)child.
        t = t->left;
        while( t->right != NULL ) {
            t = t->right;
        }
        return t;
    }
    while( t->parent != NULL && t->parent->right != t ){
        if (t->parent->left != t ) {
//            printf("runtime: predecessor t=", t, "t.spankey=", t)
            throw("node is not its parent's child")
        }
        t = t->parent;
    }
    return t->parent;
}