#ifndef LIP_TEMP_ALLOCATOR_H
#define LIP_TEMP_ALLOCATOR_H

#include "common.h"

lip_allocator_t*
lip_temp_allocator_create(lip_allocator_t* allocator, size_t chunk_size);

void
lip_temp_allocator_destroy(lip_allocator_t* temp_allocator);

void
lip_temp_allocator_reset(lip_allocator_t* temp_allocator);

#endif
