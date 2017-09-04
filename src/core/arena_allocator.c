#include "arena_allocator.h"
#include <lip/core/memory.h>

typedef struct lip_arena_allocator_s lip_arena_allocator_t;
typedef struct lip_large_alloc_s lip_large_alloc_t;
typedef struct lip_arena_chunk_s lip_arena_chunk_t;
typedef struct lip_realloc_header_s lip_realloc_header_t;

struct lip_arena_allocator_s
{
	lip_allocator_t vtable;
	lip_allocator_t* backing_allocator;
	lip_arena_chunk_t* current_chunks;
	lip_arena_chunk_t* chunks;
	lip_large_alloc_t* large_allocs;
	size_t chunk_size;
	bool relocatable;
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

struct lip_realloc_header_s
{
	size_t size;
	lip_large_alloc_t* large_alloc;
};

static const size_t lip_reallocatable_overhead =
	sizeof(lip_realloc_header_t) + LIP_ALIGN_OF(struct lip_max_align_helper) - (sizeof(lip_realloc_header_t) % LIP_ALIGN_OF(struct lip_max_align_helper));

static void*
lip_alloc_from_chunk(lip_arena_chunk_t* chunk, size_t size)
{
	char* mem = lip_align_ptr(chunk->ptr, LIP_MAX_ALIGNMENT);
	if(mem + size <= chunk->end)
	{
		chunk->ptr = mem + size;
		return mem;
	}
	else
	{
		++chunk->num_failures;
		return NULL;
	}
}

static void*
lip_arena_allocator_small_alloc(lip_arena_allocator_t* allocator, size_t size)
{
	lip_arena_chunk_t** chunkp = &allocator->current_chunks;
	for(lip_arena_chunk_t* chunk = allocator->current_chunks;
		chunk != NULL;
		chunk = chunk->next
	)
	{
		chunkp = &chunk->next;

		void* mem = lip_alloc_from_chunk(chunk, size);

		if(
			(chunk->num_failures >= 3 || chunk->ptr >= chunk->end)
			&& chunk->next
		)
		{
			allocator->current_chunks = chunk->next;
		}

		if(mem) { return mem; }
	}

	lip_arena_chunk_t* new_chunk = lip_malloc(
		allocator->backing_allocator,
		sizeof(lip_arena_chunk_t) + allocator->chunk_size
	);
	if(!new_chunk) { return NULL; }

	new_chunk->num_failures = 0;
	new_chunk->ptr = new_chunk->start;
	new_chunk->end = new_chunk->start + allocator->chunk_size;
	new_chunk->next = *chunkp;
	*chunkp = new_chunk;

	if(!allocator->chunks) { allocator->chunks = new_chunk; }

	return lip_alloc_from_chunk(new_chunk, size);
}

static void*
lip_arena_allocator_large_alloc(lip_arena_allocator_t* allocator, size_t size)
{
	void* mem = lip_malloc(allocator->backing_allocator, size);
	if(!mem) { return NULL; }

	lip_large_alloc_t* large_alloc =
		lip_arena_allocator_small_alloc(allocator, sizeof(lip_large_alloc_t));
	if(!large_alloc)
	{
		lip_free(allocator->backing_allocator, mem);
		return NULL;
	}

	large_alloc->ptr = mem;
	large_alloc->next = allocator->large_allocs;
	allocator->large_allocs = large_alloc;

	return mem;
}

static void*
lip_arena_allocator_reallocate(lip_allocator_t* vtable, void* old, size_t size)
{
	lip_arena_allocator_t* allocator =
		LIP_CONTAINER_OF(vtable, lip_arena_allocator_t, vtable);

	if(!allocator->relocatable) { return NULL; }

	size_t realloc_size = size + lip_reallocatable_overhead;

	lip_realloc_header_t* header = (void*)((char*)old - lip_reallocatable_overhead);
	if(header->large_alloc)
	{
		header = lip_realloc(
			allocator->backing_allocator, header, realloc_size
		);
		if(!header) { return NULL; }
		header->size = size;
		header->large_alloc->ptr = header;
		return (char*)header + lip_reallocatable_overhead;
	}
	else
	{
		lip_realloc_header_t* new_header =
			lip_arena_allocator_large_alloc(allocator, realloc_size);
		if(!new_header) { return NULL; }
		*new_header = (lip_realloc_header_t) {
			.size = size,
			.large_alloc = allocator->large_allocs
		};
		void* new_mem = (char*)new_header + lip_reallocatable_overhead;
		memcpy(new_mem, old, header->size);
		return new_mem;
	}
}

static void*
lip_arena_allocator_allocate(lip_allocator_t* vtable, size_t size)
{
	lip_arena_allocator_t* allocator =
		LIP_CONTAINER_OF(vtable, lip_arena_allocator_t, vtable);

	size_t alloc_size = size;
	if(allocator->relocatable) { alloc_size += lip_reallocatable_overhead; }

	void * mem;
	bool is_large_alloc = alloc_size > allocator->chunk_size;
	if(is_large_alloc)
	{
		mem = lip_arena_allocator_large_alloc(allocator, alloc_size);
	}
	else
	{
		mem = lip_arena_allocator_small_alloc(allocator, alloc_size);
	}

	if(allocator->relocatable)
	{
		lip_realloc_header_t* header = mem;
		*header = (lip_realloc_header_t){
			.size = size,
			.large_alloc = is_large_alloc ? allocator->large_allocs : NULL,
		};
		return (char*)header + lip_reallocatable_overhead;
	}
	else
	{
		return mem;
	}
}

static void*
lip_arena_allocator_realloc(lip_allocator_t* vtable, void* old, size_t size)
{
	return old
		? lip_arena_allocator_reallocate(vtable, old, size)
		: lip_arena_allocator_allocate(vtable, size);
}

static void
lip_arena_allocator_free(lip_allocator_t* vtable, void* ptr)
{
	(void)vtable;
	(void)ptr;
}

lip_allocator_t*
lip_arena_allocator_create(
	lip_allocator_t* allocator, size_t chunk_size, bool relocatable
)
{
	lip_arena_allocator_t* arena_allocator = lip_new(allocator, lip_arena_allocator_t);
	*arena_allocator = (lip_arena_allocator_t){
		.relocatable = relocatable,
		.backing_allocator = allocator,
		.current_chunks = NULL,
		.chunks = NULL,
		.large_allocs = NULL,
		.chunk_size = LIP_MAX(chunk_size, sizeof(lip_large_alloc_t)),
		.vtable = {
			.realloc = lip_arena_allocator_realloc,
			.free = lip_arena_allocator_free
		}
	};
	return &arena_allocator->vtable;
}

void
lip_arena_allocator_destroy(lip_allocator_t* allocator)
{
	lip_arena_allocator_reset(allocator);

	lip_arena_allocator_t* arena_allocator =
		LIP_CONTAINER_OF(allocator, lip_arena_allocator_t, vtable);
	for(
		lip_arena_chunk_t* chunk = arena_allocator->chunks;
		chunk != NULL;
	)
	{
		lip_arena_chunk_t* next_chunk = chunk->next;
		lip_free(arena_allocator->backing_allocator, chunk);
		chunk = next_chunk;
	}
	lip_free(arena_allocator->backing_allocator, arena_allocator);
}

void
lip_arena_allocator_reset(lip_allocator_t* vtable)
{
	lip_arena_allocator_t* arena_allocator =
		LIP_CONTAINER_OF(vtable, lip_arena_allocator_t, vtable);

	for(
		lip_large_alloc_t* large_alloc = arena_allocator->large_allocs;
		large_alloc != NULL;
		large_alloc = large_alloc->next
	)
	{
		lip_free(arena_allocator->backing_allocator, large_alloc->ptr);
	}

	arena_allocator->large_allocs = NULL;

	for(
		lip_arena_chunk_t* chunk = arena_allocator->chunks;
		chunk != NULL;
		chunk = chunk->next
	)
	{
		chunk->num_failures = 0;
		chunk->ptr = chunk->start;
	}

	arena_allocator->current_chunks = arena_allocator->chunks;
}
