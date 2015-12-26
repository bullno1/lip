#ifndef LIP_ALLOCATOR_H
#define LIP_ALLOCATOR_H

#include <stddef.h>

typedef struct lip_allocator_t
{
	void*(*realloc)(void* self, void* old, size_t size);
	void(*free)(void* self, void* mem);
} lip_allocator_t;

extern lip_allocator_t* lip_default_allocator;

#define lip_new(ALLOCATOR, TYPE) (TYPE*)(lip_malloc(ALLOCATOR, sizeof(TYPE)))

static inline void* lip_realloc(
	lip_allocator_t* allocator, void* ptr, size_t size
)
{
	return allocator->realloc(allocator, ptr, size);
}

static inline void* lip_malloc(lip_allocator_t* allocator, size_t size)
{
	return lip_realloc(allocator, 0, size);
}

static inline void lip_free(lip_allocator_t* allocator, void* ptr)
{
	allocator->free(allocator, ptr);
}

#endif
