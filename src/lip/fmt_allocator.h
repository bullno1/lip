#ifndef LIP_FMT_ALLOCATOR_H
#define LIP_FMT_ALLOCATOR_H

#include "memory.h"

lip_allocator_t*
lip_fmt_allocator_create(lip_allocator_t* allocator);

void
lip_fmt_allocator_destroy(lip_allocator_t* allocator);

#endif
