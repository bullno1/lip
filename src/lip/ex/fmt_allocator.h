#ifndef LIP_FMT_ALLOCATOR_EX_H
#define LIP_FMT_ALLOCATOR_EX_H

#include "../fmt_allocator.h"

typedef struct lip_fmt_allocator_s lip_fmt_allocator_t;

struct lip_fmt_allocator_s
{
	lip_allocator_t vtable;
	lip_allocator_t* allocator;
	size_t size;
	void* mem;
	bool freed;
};

#endif
