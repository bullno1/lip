#ifndef LIP_ARENA_ALLOCATOR_H
#define LIP_ARENA_ALLOCATOR_H

#include "common.h"

lip_allocator_t*
lip_arena_allocator_create(lip_allocator_t* allocator, size_t chunk_size);

void
lip_arena_allocator_destroy(lip_allocator_t* arena_allocator);

void
lip_arena_allocator_reset(lip_allocator_t* arena_allocator);

#endif
