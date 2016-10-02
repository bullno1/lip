#ifndef LIP_TEMP_ALLOCATOR_H
#define LIP_TEMP_ALLOCATOR_H

#include "memory.h"

lip_allocator_t*
lip_temp_allocator_create(lip_allocator_t* allocator);

void
lip_temp_allocator_destroy(lip_allocator_t* allocator);

#endif
