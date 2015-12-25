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
	(TYPE*)(ALLOCATOR->vtable->malloc(allocator->context, sizeof(TYPE)))
#define LIP_DELETE(ALLOCATOR, OBJ) \
	ALLOCATOR->vtable->free(allocator->context, OBJ)

extern lip_allocator_t lip_default_allocator;

#endif
