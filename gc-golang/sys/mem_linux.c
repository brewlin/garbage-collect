#include "sysapi.h"
#include "gc.h"
#include "../malloc/fixalloc.h"
#include "gpm.h"
#include "../atomic/atomic.h"
#include "platform.h"
#include "../malloc/malloc.h"
#include <sys/mman.h>
#include <unistd.h>

#define _EACCES 13
#define _EINVAL 22

/**
 *
 * @param size
 * @param align
 * @return
 */
void* sys_fixalloc(uintptr size,uintptr align)
{
	void *p;
	uint64 maxBlock = 64 << 10; // VM reservation granularity is 64K on windows

	if ( size == 0 ) {
		throw("persistentalloc: size == 0");
	}
	if ( align != 0 ) {
		if ( align&(align-1) != 0 ){
			throw("persistentalloc: align is not a power of 2");
		}
		if ( align > _PageSize ){
			throw("persistentalloc: align is too large");
		}
	} else {
		align = 8;
	}

	//如果大于预留的内存则直接 os 申请
	if ( size >= maxBlock ){
		return sys_alloc(size);
	}

	m* mp = acquirem();

	palloc* persistent;
	if ( mp != NULL && mp->p != 0 ){
		persistent = &mp->p->pl;
	} else {
		lock(&ga_lock);
		persistent = &globalAlloc;
	}
	persistent->off = round(persistent->off, align);
	if ( persistent->off + size > persistentChunkSize || persistent->base == NULL ){
		persistent->base = sys_alloc(persistentChunkSize);
		//申请失败了
		if ( persistent->base == NULL ){
			if ( persistent == &globalAlloc ){
				unlock(&ga_lock);
			}
			throw("runtime: cannot allocate memory");
		}

		// Add the new chunk to the persistentChunks list.
		while(true) {
			uintptr  chunks = (uintptr)((void*)(persistentChunks));
			*(uintptr*)((void*)(persistent->base)) = chunks;
			if ( atomic_Casuintptr((uintptr*)((void*)(&persistentChunks)), chunks, (uintptr)((void*)(persistent->base)))){
				break;
			}
		}
		persistent->off = ptrSize;
	}
	p = persistent->base + persistent->off;
	persistent->off += size;
	releasem(mp);
	if ( persistent == &globalAlloc ){
		unlock(&ga_lock);
	}
	return p;
}


uint32 adviseUnused = (uint32)(_MADV_FREE);
/**
 * @param n
 * @return
 */
void* sys_alloc(uintptr n)
{
	void *p = mmap(NULL, n, _PROT_READ|_PROT_WRITE, _MAP_ANON|_MAP_PRIVATE, -1, 0);
	if ( p == _EACCES) {
		throw("runtime: mmap: access denied\n");
	}
	if ( p == _EAGAIN) {
		throw("runtime: mmap: too much locked memory (check 'ulimit -l').\n");
	}
	return p;
}

/**
 *
 * @param v
 * @param n
 */
void sys_unused(void* v,uintptr n)
{

	if ( HugePageSize != 0) {
		uintptr  s = HugePageSize,head,tail;

		if ( (uintptr)(v)%s != 0) {
			head = (uintptr)(v) &~ (s - 1);
		}
		if ( ((uintptr)(v)+n)%s != 0) {
			tail = ((uintptr)(v) + n - 1) &~ (s - 1);
		}

		if ( head != 0 && head + HugePageSize == tail) {
			madvise((void*)head, 2*HugePageSize, _MADV_NOHUGEPAGE);
		} else {
			if ( head != 0) {
				madvise((void*)head, HugePageSize, _MADV_NOHUGEPAGE);
			}
			if ( tail != 0 && tail != head) {
				madvise((void*)tail, HugePageSize, _MADV_NOHUGEPAGE);
			}
		}
	}

	if ( (uintptr)(v)&(physPageSize-1) != 0 || n&(physPageSize-1) != 0) {
		throw("unaligned sysUnused");
	}

	uint32 advise;
//	if ( debug.madvdontneed != 0) {
//		advise = _MADV_DONTNEED;
//	} else) {
//		advise = atomic_Load(&adviseUnused);
    advise = adviseUnused;
//	}
    void* p = madvise(v, n, (int32)(advise));
    if ( advise == _MADV_FREE && p == 0) {
		atomic_Store(&adviseUnused, _MADV_DONTNEED);
		madvise(v, n, _MADV_DONTNEED);
	}
}
/**
 *
 * @param v
 * @param n
 */
void sys_used(void* v,uintptr n)
{
	
	if ( HugePageSize != 0) {
	    
		uintptr s = HugePageSize,beg,end;

		// Round v up to a huge page boundary.
		beg = ((uintptr)(v) + (s - 1)) &~ (s - 1);
		// Round v+n down to a huge page boundary.
		end = ((uintptr)(v) + n) &~ (s - 1);

		if ( beg < end) {
			madvise((void*)(beg), end-beg, _MADV_HUGEPAGE);
		}
	}
}
/**
 *
 * @param v
 * @param n
 */
void sys_free(void* v,uintptr n)
{
	munmap(v, n);
}
/**
 *
 * @param v
 * @param n
 */
void sys_fault(void* v,uintptr n)
{
	mmap(v, n, _PROT_NONE, _MAP_ANON|_MAP_PRIVATE|_MAP_FIXED, -1, 0);
}
/**
 *
 * @param v
 * @param n
 * @return
 */
void* sys_reserve(void* v,uintptr n)
{
	void* p = mmap(v, n, _PROT_NONE, _MAP_ANON|_MAP_PRIVATE, -1, 0);
	if ( p == 0) {
		return NULL;
	}
	return p;
}

// sysReserveAligned is like sysReserve, but the returned pointer is
// aligned to align bytes. It may reserve either n or n+align bytes,
// so it returns the size that was reserved.
/**
 *
 * @param v
 * @param size
 * @param align
 * @return
 */
void* sys_reserveAligned(void* v,uintptr* ssize,uintptr align)
{

	// function, we're not likely to get it by chance, so we ask
	// for a larger region and remove the parts we don't need.
	uintptr size,end,p,pAligned,endLen;

	size = *ssize;
	int retries = 0;
retry:
	p = (uintptr)(sys_reserve(v, size+align));
	if(p == 0) {
        return NULL;
    }
	if( p&(align-1) == 0) {
        // We got lucky and got an aligned region, so we can
        // use the whole thing.
        *ssize = size + align;
        return p;
    }
//	case GOOS == "windows":
//		return p2, size
		// Trim off the unaligned parts.
    pAligned = round(p, align);
    sys_free(p,pAligned-p);
    end = pAligned + size;
    endLen = (p + size + align) - end;
    if (endLen > 0 ){
        sys_free(end,endLen);
    }
    return pAligned;
}

void sys_map(void* v,uintptr n)
{
    void * p = mmap(v, n, _PROT_READ|_PROT_WRITE, _MAP_ANON|_MAP_FIXED|_MAP_PRIVATE, -1, 0);
	if ( p == _ENOMEM) {
		printf("runtime: out of memory");
		exit(-1);
	}
	if ( p != v) {
		printf("runtime: cannot map pages in arena address space\n");
		exit(-1);
	}
}
