#ifndef LIP_ALLOCATOR_H
#define LIP_ALLOCATOR_H

#include <stdlib.h>

typedef struct lip_allocator_vtable_t
{
	void*(*malloc)(void* context, size_t size);
	void(*free)(void* context, void* mem);
} lip_allocator_vtable_t;

typedef struct lip_allocator_t
{
	void* context;
	lip_allocator_vtable_t* vtable;
} lip_allocator_t;

#define LIP_NEW(ALLOCATOR, TYPE) \
	(TYPE*)(LIP_MALLOC(ALLOCATOR, sizeof(TYPE)))
#define LIP_DELETE LIP_FREE
#define LIP_MALLOC(ALLOCATOR, SIZE) \
	(ALLOCATOR->vtable->malloc(ALLOCATOR->context, SIZE))
#define LIP_FREE(ALLOCATOR, MEM) \
	ALLOCATOR->vtable->free(ALLOCATOR->context, MEM)

extern lip_allocator_t lip_default_allocator;

#endif
