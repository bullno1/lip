#ifndef LIP_TEMP_ALLOCATOR_EX_H
#define LIP_TEMP_ALLOCATOR_EX_H

#include "../temp_allocator.h"

typedef struct lip_temp_allocator_s lip_temp_allocator_t;

struct lip_temp_allocator_s
{
	lip_allocator_t vtable;
	lip_allocator_t* allocator;
	size_t size;
	void* mem;
	bool freed;
};

void
lip_temp_allocator_init(
	lip_temp_allocator_t* allocator, lip_allocator_t* backing_allocator
);

void
lip_temp_allocator_cleanup(lip_temp_allocator_t* allocator);

#endif
