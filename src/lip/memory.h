#ifndef LIP_MEMORY_H
#define LIP_MEMORY_H

#include "common.h"

#define LIP_ALIGN_OF(TYPE) offsetof(struct { char c; TYPE t;}, t)

struct lip_allocator_s
{
	void*(*realloc)(void* self, void* old, size_t size);
	void(*free)(void* self, void* mem);
};

extern lip_allocator_t* lip_default_allocator;

#define lip_new(ALLOCATOR, TYPE) (TYPE*)(lip_malloc(ALLOCATOR, sizeof(TYPE)))

static inline void*
lip_realloc(lip_allocator_t* allocator, void* ptr, size_t size)
{
	return allocator->realloc(allocator, ptr, size);
}

static inline void*
lip_malloc(lip_allocator_t* allocator, size_t size)
{
	return lip_realloc(allocator, 0, size);
}

static inline void
lip_free(lip_allocator_t* allocator, void* ptr)
{
	allocator->free(allocator, ptr);
}

#endif
