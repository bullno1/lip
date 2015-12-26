#ifndef LIP_ALLOCATOR_H
#define LIP_ALLOCATOR_H

#include <stdlib.h>

typedef struct lip_allocator_t
{
	void*(*realloc)(void* self, void* old, size_t size);
	void(*free)(void* self, void* mem);
} lip_allocator_t;

#define LIP_NEW(ALLOCATOR, TYPE) (TYPE*)(LIP_MALLOC(ALLOCATOR, sizeof(TYPE)))
#define LIP_DELETE LIP_FREE
#define LIP_MALLOC(ALLOCATOR, SIZE) LIP_REALLOC(ALLOCATOR, 0, SIZE)
#define LIP_REALLOC(ALLOCATOR, PTR, SIZE) (ALLOCATOR->realloc(ALLOCATOR, PTR, SIZE))
#define LIP_FREE(ALLOCATOR, MEM) (ALLOCATOR->free(ALLOCATOR, MEM))

extern lip_allocator_t* lip_default_allocator;

#endif
