#ifndef LIP_ARENA_ALLOCATOR_EX_H
#define LIP_ARENA_ALLOCATOR_EX_H

#include "../arena_allocator.h"
#include "../memory.h"

typedef struct lip_arena_allocator_s lip_arena_allocator_t;
typedef struct lip_large_alloc_s lip_large_alloc_t;
typedef struct lip_arena_chunk_s lip_arena_chunk_t;

struct lip_arena_allocator_s
{
	lip_allocator_t vtable;
	lip_allocator_t* backing_allocator;
	lip_arena_chunk_t* current_chunks;
	lip_arena_chunk_t* chunks;
	lip_large_alloc_t* large_allocs;

	size_t chunk_size;
};

struct lip_large_alloc_s
{
	void* ptr;
	void* next;
};

struct lip_arena_chunk_s
{
	char num_failures;
	lip_arena_chunk_t* next;
	char* ptr;
	char* end;
	char start[];
};

void
lip_arena_allocator_init(
	lip_arena_allocator_t* arena_allocator,
	lip_allocator_t* backing_allocator,
	size_t chunk_size
);

void
lip_arena_allocator_cleanup(lip_arena_allocator_t* arena_allocator);

#endif
