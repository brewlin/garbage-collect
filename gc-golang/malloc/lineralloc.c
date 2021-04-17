/**
 *@ClassName lineralloc
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/31 0031 下午 4:18
 *@Version 1.0
 **/
#include "lineralloc.h"
#include "../sys/gc.h"
#include "../sys/sysapi.h"
#include "malloc.h"

/**
 *
 * @param l
 * @param size
 * @param align
 * @return
 */
void* linearalloc_alloc(lineralloc* l,uintptr size,uintptr align)
{

	uintptr p = round(l->next, align);
	if ( p+size > l->end ){
		return NULL;
	}
	l->next = p + size;
    uintptr pEnd = round(l->next-1, physPageSize);
    if ( pEnd > l->mapped ){
		// We need to map more of the reserved space.
		sys_map(l->mapped,pEnd-l->mapped);
		l->mapped = pEnd;
	}
	return p;
}