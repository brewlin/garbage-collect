#include "obmalloc.h"

//多空闲链表，加开内存分配速度 基本上O(1)
#define PTA(x)	((poolp )((uchar *)&(usedpools[2*(x)]) - 2*sizeof(block *)))
#define PT(x)	PTA(x), PTA(x)
//小技巧: 在pool内分配的内存都可以通过下面立马计算出 pool_header的地址，非常高效
#define POOL_ADDR(P) ((poolp)((uptr)(P) & ~(uptr)POOL_SIZE_MASK))
//判断指针是否合法
#define Py_ADDRESS_IN_RANGE(P, POOL)			\
	((POOL)->arenaindex < maxarenas &&		\
	 (uptr)(P) - arenas[(POOL)->arenaindex].address < (uptr)ARENA_SIZE && \
	 arenas[(POOL)->arenaindex].address != 0)


//为了快速查询， 设置了多个空闲链表 multi_free_list 0 对应8字节 1 对16字节。。。
//共64个大小
 poolp usedpools[2 * ((NB_SMALL_SIZE_CLASSES + 7) / 8) * 8] = {
	PT(0), PT(1), PT(2), PT(3), PT(4), PT(5), PT(6), PT(7),
	PT(8), PT(9), PT(10), PT(11), PT(12), PT(13), PT(14), PT(15),
	PT(16), PT(17), PT(18), PT(19), PT(20), PT(21), PT(22), PT(23),
	PT(24), PT(25), PT(26), PT(27), PT(28), PT(29), PT(30), PT(31)
};

/*==========================================================================
unused_arena_objects
	作为一个单链表，链表的所有object都没有关联真正的arena内存，

usable_arenas
	指向一个双向链表，关联着包含可用pool的arena内存块，这些pool要么没被用过，
	要么在等待reuse->freeBlock
*/

//全局arena vector 数组
 struct arena_object* arenas = NULL;
//当前在areanas中分配的最大slot索引
 uint maxarenas = 0;

 struct arena_object* unused_arena_objects = NULL;

 //双向链表，将由可用pool内存块的arena 串起来
 struct arena_object* usable_arenas = NULL;

//初始化16个arena_object 数组
#define INITIAL_ARENA_OBJECTS 16

//已经分配了arena内存 但是还没有释放的数量
size_t narenas_currently_allocated = 0;

// arena -   pool   -  block
// arena 一般是 256k  最大的一个层级  多个arena由双向链表串联起来
// pool  一般是 4k 属于一个arena里面，并且多个pool通过链表串联起来
// block 是pool上的最小分配内存  由用户结构体而定义
struct arena_object* new_arena(void)
{
	struct arena_object* arenaobj;
	uint excess;	/* number of bytes above pool alignment */

	//如果一个可用的arena都没有，说明程序还没有初始化 需要走初始化流程
	if (unused_arena_objects == NULL) {
		uint i;
		uint numarenas;
		size_t nbytes;

		//1 第一次初始化的时候 numarenas = 0 所以默认初始化16个大小的数组
		//2 第二次来说明空间不够了 numareans *= 2 因为无符号为2字节大小，所以numarenas 可能会溢出
		numarenas = maxarenas ? maxarenas << 1 : INITIAL_ARENA_OBJECTS;
		//溢出了
		if (numarenas <= maxarenas)
			return NULL;	/* overflow */
		nbytes = numarenas * sizeof(*arenas);
		//扩充之间的数组 并返回了新的指针执行扩充后的新空间
		arenaobj = (struct arena_object *)realloc(arenas, nbytes);
		if (arenaobj == NULL)
			return NULL;
		//替换新的全局arean数组
		arenas = arenaobj;

		//还是要验证下 确保到这里的时候可用的arean都为空
		assert(usable_arenas == NULL);
		assert(unused_arena_objects == NULL);

		//arenas 数组还没有初始化 现在进行初始化
		for (i = maxarenas; i < numarenas; ++i) {
			arenas[i].address = 0;	//标记还没有初始化256k内存空间
			arenas[i].nextarena = i < numarenas - 1 ?
					       &arenas[i+1] : NULL;
		}
		//未使用空间指向之前的后一个，第一次的时候这个数值为0
		unused_arena_objects = &arenas[maxarenas];
		//更新areans数量
		maxarenas = numarenas;
	}

	assert(unused_arena_objects != NULL);
	arenaobj = unused_arena_objects;
	unused_arena_objects = arenaobj->nextarena;
	//确保当前的areanobj 还没有初始化内存
	assert(arenaobj->address == 0);
	//分配一个 arena内存池 一般 256k大小
	arenaobj->address = (uptr)malloc(ARENA_SIZE);
	if (arenaobj->address == 0) {
		//分配失败了 需要返回NULL 并且将arenaobj推到链表头 等待下次继续初始化
		arenaobj->nextarena = unused_arena_objects;
		unused_arena_objects = arenaobj;
		return NULL;
	}

	++narenas_currently_allocated;

	arenaobj->freepools = NULL;
	arenaobj->pool_address = (block*)arenaobj->address;
	arenaobj->nfreepools = ARENA_SIZE / POOL_SIZE;
	assert(POOL_SIZE * arenaobj->nfreepools == ARENA_SIZE);
	//pool需要内存对齐，所以这个时候就是计算pool对齐后的地址
	excess = (uint)(arenaobj->address & POOL_SIZE_MASK);
	if (excess != 0) {
		--arenaobj->nfreepools;
		arenaobj->pool_address += POOL_SIZE - excess;
	}
	arenaobj->ntotalpools = arenaobj->nfreepools;

	return arenaobj;
}
//执行内存分配逻辑
void * PyObject_Malloc(size_t nbytes)
{
	block *bp;
	//pool空闲链表池
	poolp pool;
	poolp next;
	uint size;


	 //为了安全漏洞 阻止申请超过 最大申请内存
	if (nbytes > PY_SSIZE_T_MAX)
		return NULL;

	//隐式的重定向到了 malloc
	if ((nbytes - 1) < SMALL_REQUEST_THRESHOLD) {
	    //加锁

		//将申请的内存进行对齐 然后换算成空闲pools上的索引
		size = (uint)(nbytes - 1) >> ALIGNMENT_SHIFT;
		pool = usedpools[size + size];
		if (pool != pool->nextpool) {
			//该pool内 分配的block数量 +1 维护已经分配的block总数
			++pool->ref.count;
			//从block 空闲链表上查找，就像我们自己实现的那个free_list 一个意思
			bp = pool->freeblock;
			assert(bp != NULL);
			//这里表示 pol->freeblock = bp->next
			//*(block **)bp 是为了节省next指针 而直接在里面保存了下一个指针
			if ((pool->freeblock = *(block **)bp) != NULL) {
			    //解锁
				return (void *)bp;
			}
			//上面说明freeblock 已经没有空闲链表了，现在需要额外分配新的内存
			if (pool->nextoffset <= pool->maxnextoffset) {
				/* There is room for another block. */
				pool->freeblock = (block*)pool + pool->nextoffset;
				//nexttoffset 执行下一个未分配地址开头
				pool->nextoffset += INDEX2SIZE(size);
				//相当于 pool->freeblock->next = NULL
				*(block **)(pool->freeblock) = NULL;
				//解锁
				return (void *)bp;
			}
			//到这里说明两个问题
			//1 freeblock 没有空闲block可用
			//2 pool也分配耗尽了
			//接下来就要将这个pool从全局pool空闲链表上面移除
			next = pool->nextpool;
			pool = pool->prevpool;
			next->prevpool = pool;
			pool->nextpool = next;
			//解锁
			return (void *)bp;
		}

		//说明一个pool都没有了 且可用的arenas也没有了
		if (usable_arenas == NULL) {
			//现在需要新申请一个 arena
			usable_arenas = new_arena();
			if (usable_arenas == NULL) {
				//解锁
				//如果依然申请失败 则直接调用redirect直接malloc申请内存
				goto redirect;
			}
			//设置当前arena next 和 prev 都为空
			usable_arenas->nextarena =
				usable_arenas->prevarena = NULL;
		}
		//这里判断arena内存池是否真正申请成功
		assert(usable_arenas->address != 0);

		/* Try to get a cached free pool. */
		pool = usable_arenas->freepools;
		if (pool != NULL) {
			/* Unlink from cached pools. */
			usable_arenas->freepools = pool->nextpool;

			/* This arena already had the smallest nfreepools
			 * value, so decreasing nfreepools doesn't change
			 * that, and we don't need to rearrange the
			 * usable_arenas list.  However, if the arena has
			 * become wholly allocated, we need to remove its
			 * arena_object from usable_arenas.
			 */
			--usable_arenas->nfreepools;
			if (usable_arenas->nfreepools == 0) {
				/* Wholly allocated:  remove. */
				assert(usable_arenas->freepools == NULL);
				assert(usable_arenas->nextarena == NULL ||
				       usable_arenas->nextarena->prevarena ==
					   usable_arenas);

				usable_arenas = usable_arenas->nextarena;
				if (usable_arenas != NULL) {
					usable_arenas->prevarena = NULL;
					assert(usable_arenas->address != 0);
				}
			}
			else {
				/* nfreepools > 0:  it must be that freepools
				 * isn't NULL, or that we haven't yet carved
				 * off all the arena's pools for the first
				 * time.
				 */
				assert(usable_arenas->freepools != NULL ||
				       usable_arenas->pool_address <=
				           (block*)usable_arenas->address +
				               ARENA_SIZE - POOL_SIZE);
			}
		init_pool:
			//将新的空闲的pool 挂到  全局 pool链表上去
			next = usedpools[size + size]; /* == prev */
			pool->nextpool = next;
			pool->prevpool = next;
			next->nextpool = pool;
			next->prevpool = pool;
			pool->ref.count = 1;
			if (pool->szidx == size) {
				/* Luckily, this pool last contained blocks
				 * of the same size class, so its header
				 * and free list are already initialized.
				 */
				bp = pool->freeblock;
				pool->freeblock = *(block **)bp;
				//解锁
				return (void *)bp;
			}
			/*
			 * Initialize the pool header, set up the free list to
			 * contain just the second block, and return the first
			 * block.
			 */
			pool->szidx = size;
			size = INDEX2SIZE(size);
			bp = (block *)pool + POOL_OVERHEAD;
			pool->nextoffset = POOL_OVERHEAD + (size << 1);
			pool->maxnextoffset = POOL_SIZE - size;
			pool->freeblock = bp + size;
			*(block **)(pool->freeblock) = NULL;
			//解锁
			return (void *)bp;
		}

		//开辟新的内存池
		assert(usable_arenas->nfreepools > 0);
		assert(usable_arenas->freepools == NULL);
		//因为新申请的arenas 是一块256k的大内存，还没切分为小的pool
		pool = (poolp)usable_arenas->pool_address;
		assert((block*)pool <= (block*)usable_arenas->address +
		                       ARENA_SIZE - POOL_SIZE);
		pool->arenaindex = usable_arenas - arenas;
		assert(&arenas[pool->arenaindex] == usable_arenas);
		pool->szidx = DUMMY_SIZE_IDX;
		//正在使用中的arena pool地址指向下一个pool
		usable_arenas->pool_address += POOL_SIZE;
		--usable_arenas->nfreepools;

		if (usable_arenas->nfreepools == 0) {
			assert(usable_arenas->nextarena == NULL ||
			       usable_arenas->nextarena->prevarena ==
			       	   usable_arenas);
			/* Unlink the arena:  it is completely allocated. */
			//当前arena 已经完全用完了 需要指向一下一个可用的arena
			usable_arenas = usable_arenas->nextarena;
			if (usable_arenas != NULL) {
				usable_arenas->prevarena = NULL;
				assert(usable_arenas->address != 0);
			}
		}

		goto init_pool;
	}

        /* The small block allocator ends here. */

redirect:
	/* Redirect the original request to the underlying (libc) allocator.
	 * We jump here on bigger requests, on error in the code above (as a
	 * last chance to serve the request) or when the max memory limit
	 * has been reached.
	 */
	if (nbytes == 0)
		nbytes = 1;
	return (void *)malloc(nbytes);
}

/* free */

#undef PyObject_Free

void PyObject_Free(void *p)
{
	poolp pool;
	block *lastfree;
	poolp next, prev;
	uint size;

	if (p == NULL)	/* free(NULL) has no effect */
		return;
	//这里是查找p 所属的pool
	pool = POOL_ADDR(p);
	//#define POOL_ADDR(P) ((poolp)((uptr)(P) & ~(uptr)POOL_SIZE_MASK))
	//#define POOL_ADDR(P) (P & 0xfffff000)
	if (Py_ADDRESS_IN_RANGE(p, pool)) {
		/* We allocated this address. */
		//加锁
		/* Link p to the start of the pool's freeblock list.  Since
		 * the pool had at least the p block outstanding, the pool
		 * wasn't empty (so it's already in a usedpools[] list, or
		 * was full and is in no list -- it's not in the freeblocks
		 * list in any case).
		 */
		//既然p属于 pool里面，那么pool必然已使用block 肯定大于0
		assert(pool->ref.count > 0);	/* else it was empty */
		//将当前p挂到 pool->freeblokc->next 下面
		*(block **)p = lastfree = pool->freeblock;
		//然后pool->freeblock = p
		pool->freeblock = (block *)p;
		if (lastfree) {
			struct arena_object* ao;
			uint nf;  /* ao->nfreepools */

			//freeblock 不为空，说明pool还没满
			if (--pool->ref.count != 0) {
				/* pool isn't empty:  leave it in usedpools */
				//解锁
				return;
			}
			/* Pool is now empty:  unlink from usedpools, and
			 * link to the front of freepools.  This ensures that
			 * previously freed pools will be allocated later
			 * (being not referenced, they are perhaps paged out).
			 */
			//pool现在为空，需要从空闲链表上拿出去
			next = pool->nextpool;
			prev = pool->prevpool;
			next->prevpool = prev;
			prev->nextpool = next;

			/* Link the pool to freepools.  This is a singly-linked
			 * list, and pool->prevpool isn't used there.
			 */
			ao = &arenas[pool->arenaindex];
			pool->nextpool = ao->freepools;
			ao->freepools = pool;
			nf = ++ao->nfreepools;

			/* All the rest is arena management.  We just freed
			 * a pool, and there are 4 cases for arena mgmt:
			 * 1. If all the pools are free, return the arena to
			 *    the system free().
			 * 2. If this is the only free pool in the arena,
			 *    add the arena back to the `usable_arenas` list.
			 * 3. If the "next" arena has a smaller count of free
			 *    pools, we have to "slide this arena right" to
			 *    restore that usable_arenas is sorted in order of
			 *    nfreepools.
			 * 4. Else there's nothing more to do.
			 */
			if (nf == ao->ntotalpools) {
				/* Case 1.  First unlink ao from usable_arenas.
				 */
				assert(ao->prevarena == NULL ||
				       ao->prevarena->address != 0);
				assert(ao ->nextarena == NULL ||
				       ao->nextarena->address != 0);

				/* Fix the pointer in the prevarena, or the
				 * usable_arenas pointer.
				 */
				if (ao->prevarena == NULL) {
					usable_arenas = ao->nextarena;
					assert(usable_arenas == NULL ||
					       usable_arenas->address != 0);
				}
				else {
					assert(ao->prevarena->nextarena == ao);
					ao->prevarena->nextarena =
						ao->nextarena;
				}
				/* Fix the pointer in the nextarena. */
				if (ao->nextarena != NULL) {
					assert(ao->nextarena->prevarena == ao);
					ao->nextarena->prevarena =
						ao->prevarena;
				}
				/* Record that this arena_object slot is
				 * available to be reused.
				 */
				ao->nextarena = unused_arena_objects;
				unused_arena_objects = ao;

				/* Free the entire arena. */
				free((void *)ao->address);
				ao->address = 0;	/* mark unassociated */
				--narenas_currently_allocated;

				//解锁
				return;
			}
			if (nf == 1) {
				/* Case 2.  Put ao at the head of
				 * usable_arenas.  Note that because
				 * ao->nfreepools was 0 before, ao isn't
				 * currently on the usable_arenas list.
				 */
				ao->nextarena = usable_arenas;
				ao->prevarena = NULL;
				if (usable_arenas)
					usable_arenas->prevarena = ao;
				usable_arenas = ao;
				assert(usable_arenas->address != 0);

				//解锁
				return;
			}
			/* If this arena is now out of order, we need to keep
			 * the list sorted.  The list is kept sorted so that
			 * the "most full" arenas are used first, which allows
			 * the nearly empty arenas to be completely freed.  In
			 * a few un-scientific tests, it seems like this
			 * approach allowed a lot more memory to be freed.
			 */
			if (ao->nextarena == NULL ||
				     nf <= ao->nextarena->nfreepools) {
				/* Case 4.  Nothing to do. */
				//解锁
				return;
			}
			/* Case 3:  We have to move the arena towards the end
			 * of the list, because it has more free pools than
			 * the arena to its right.
			 * First unlink ao from usable_arenas.
			 */
			if (ao->prevarena != NULL) {
				/* ao isn't at the head of the list */
				assert(ao->prevarena->nextarena == ao);
				ao->prevarena->nextarena = ao->nextarena;
			}
			else {
				/* ao is at the head of the list */
				assert(usable_arenas == ao);
				usable_arenas = ao->nextarena;
			}
			ao->nextarena->prevarena = ao->prevarena;

			/* Locate the new insertion point by iterating over
			 * the list, using our nextarena pointer.
			 */
			while (ao->nextarena != NULL &&
					nf > ao->nextarena->nfreepools) {
				ao->prevarena = ao->nextarena;
				ao->nextarena = ao->nextarena->nextarena;
			}

			/* Insert ao at this point. */
			assert(ao->nextarena == NULL ||
				ao->prevarena == ao->nextarena->prevarena);
			assert(ao->prevarena->nextarena == ao->nextarena);

			ao->prevarena->nextarena = ao;
			if (ao->nextarena != NULL)
				ao->nextarena->prevarena = ao;

			/* Verify that the swaps worked. */
			assert(ao->nextarena == NULL ||
				  nf <= ao->nextarena->nfreepools);
			assert(ao->prevarena == NULL ||
				  nf > ao->prevarena->nfreepools);
			assert(ao->nextarena == NULL ||
				ao->nextarena->prevarena == ao);
			assert((usable_arenas == ao &&
				ao->prevarena == NULL) ||
				ao->prevarena->nextarena == ao);

			//解锁
			return;
		}
		/* Pool was full, so doesn't currently live in any list:
		 * link it to the front of the appropriate usedpools[] list.
		 * This mimics LRU pool usage for new allocations and
		 * targets optimal filling when several pools contain
		 * blocks of the same size class.
		 */
		--pool->ref.count;
		assert(pool->ref.count > 0);	/* else the pool is empty */
		size = pool->szidx;
		next = usedpools[size + size];
		prev = next->prevpool;
		/* insert pool before next:   prev <-> pool <-> next */
		pool->nextpool = next;
		pool->prevpool = prev;
		next->prevpool = pool;
		prev->nextpool = pool;
		//解锁
		return;
	}

	//说明不是通过asrena上申请的内存，直接free即可
	free(p);
}

/* realloc.  If p is NULL, this acts like malloc(nbytes).  Else if nbytes==0,
 * then as the Python docs promise, we do not treat this like free(p), and
 * return a non-NULL result.
 */

#undef PyObject_Realloc
void* PyObject_Realloc(void *p, size_t nbytes)
{
	void *bp;
	poolp pool;
	size_t size;

	if (p == NULL)
		return PyObject_Malloc(nbytes);

	/*
	 * Limit ourselves to PY_SSIZE_T_MAX bytes to prevent security holes.
	 * Most python internals blindly use a signed Py_ssize_t to track
	 * things without checking for overflows or negatives.
	 * As size_t is unsigned, checking for nbytes < 0 is not required.
	 */
	if (nbytes > PY_SSIZE_T_MAX)
		return NULL;

	pool = POOL_ADDR(p);
	if (Py_ADDRESS_IN_RANGE(p, pool)) {
		/* We're in charge of this block */
		size = INDEX2SIZE(pool->szidx);
		if (nbytes <= size) {
			/* The block is staying the same or shrinking.  If
			 * it's shrinking, there's a tradeoff:  it costs
			 * cycles to copy the block to a smaller size class,
			 * but it wastes memory not to copy it.  The
			 * compromise here is to copy on shrink only if at
			 * least 25% of size can be shaved off.
			 */
			if (4 * nbytes > 3 * size) {
				/* It's the same,
				 * or shrinking and new/old > 3/4.
				 */
				return p;
			}
			size = nbytes;
		}
		bp = PyObject_Malloc(nbytes);
		if (bp != NULL) {
			memcpy(bp, p, size);
			PyObject_Free(p);
		}
		return bp;
	}
	/* We're not managing this block.  If nbytes <=
	 * SMALL_REQUEST_THRESHOLD, it's tempting to try to take over this
	 * block.  However, if we do, we need to copy the valid data from
	 * the C-managed block to one of our blocks, and there's no portable
	 * way to know how much of the memory space starting at p is valid.
	 * As bug 1185883 pointed out the hard way, it's possible that the
	 * C-managed block is "at the end" of allocated VM space, so that
	 * a memory fault can occur if we try to copy nbytes bytes starting
	 * at p.  Instead we punt:  let C continue to manage this block.
         */
	if (nbytes)
		return realloc(p, nbytes);
	/* C doesn't define the result of realloc(p, 0) (it may or may not
	 * return NULL then), but Python's docs promise that nbytes==0 never
	 * returns NULL.  We don't pass 0 to realloc(), to avoid that endcase
	 * to begin with.  Even then, we can't be sure that realloc() won't
	 * return NULL.
	 */
	bp = realloc(p, 1);
   	return bp ? bp : p;
}


//void* PyObject_Malloc(size_t n){
//	return malloc(n);
//}
//void* PyObject_Realloc(void *p, size_t n){
//	return realloc(p, n);
//}
//void PyObject_Free(void *p){
//	free(p);
//}


