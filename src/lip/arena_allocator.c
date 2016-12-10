#include "arena_allocator.h"
#include "memory.h"

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
lip_arena_allocator_realloc(lip_allocator_t* vtable, void* old, size_t size)
{
	// This allocator cannot do reallocation
	if(old) { return NULL; }

	lip_arena_allocator_t* allocator =
		LIP_CONTAINER_OF(vtable, lip_arena_allocator_t, vtable);

	if(size > allocator->chunk_size)
	{
		return lip_arena_allocator_large_alloc(allocator, size);
	}
	else
	{
		return  lip_arena_allocator_small_alloc(allocator, size);
	}
}

static void
lip_arena_allocator_free(lip_allocator_t* vtable, void* ptr)
{
	(void)vtable;
	(void)ptr;
}

lip_allocator_t*
lip_arena_allocator_create(lip_allocator_t* allocator, size_t chunk_size)
{
	lip_arena_allocator_t* arena_allocator =
		lip_new(allocator, lip_arena_allocator_t);
	arena_allocator->vtable.realloc = lip_arena_allocator_realloc;
	arena_allocator->vtable.free = lip_arena_allocator_free;
	arena_allocator->backing_allocator = allocator;
	arena_allocator->current_chunks = NULL;
	arena_allocator->chunks = NULL;
	arena_allocator->large_allocs = NULL;
	arena_allocator->chunk_size = LIP_MAX(chunk_size, sizeof(lip_large_alloc_t));
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
