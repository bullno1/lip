#ifndef LIP_TEMP_ALLOCATOR_EX_H
#define LIP_TEMP_ALLOCATOR_EX_H

#include "../temp_allocator.h"
#include "../memory.h"

typedef struct lip_temp_allocator_s lip_temp_allocator_t;
typedef struct lip_large_alloc_s lip_large_alloc_t;
typedef struct lip_temp_chunk_s lip_temp_chunk_t;

struct lip_temp_allocator_s
{
	lip_allocator_t vtable;
	lip_allocator_t* backing_allocator;
	lip_temp_chunk_t* current_chunks;
	lip_temp_chunk_t* chunks;
	lip_large_alloc_t* large_allocs;

	size_t chunk_size;
};

struct lip_large_alloc_s
{
	void* ptr;
	void* next;
};

struct lip_temp_chunk_s
{
	char num_failures;
	lip_temp_chunk_t* next;
	char* ptr;
	char* end;
	char start[];
};

#endif
