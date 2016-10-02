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

#endif
