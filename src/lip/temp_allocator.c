#include "ex/temp_allocator.h"

static void*
lip_temp_allocator_realloc(lip_allocator_t* vtable, void* old, size_t size)
{
	lip_temp_allocator_t* temp_allocator =
		LIP_CONTAINER_OF(vtable, lip_temp_allocator_t, vtable);

	if(!(false
		|| (old != NULL && old == temp_allocator->mem) // realloc
		|| (old == NULL && temp_allocator->freed))) // malloc
	{
		return NULL;
	}

	temp_allocator->freed = false;
	void* mem = NULL;
	if(temp_allocator->size < size)
	{
		mem = lip_realloc(temp_allocator->allocator, temp_allocator->mem, size);
		temp_allocator->mem = mem;
		temp_allocator->size = size;
	}
	else
	{
		mem = temp_allocator->mem;
	}

	return mem;
}

static void
lip_temp_allocator_free(lip_allocator_t* vtable, void* mem)
{
	lip_temp_allocator_t* temp_allocator =
		LIP_CONTAINER_OF(vtable, lip_temp_allocator_t, vtable);
	if(mem == temp_allocator->mem)
	{
		temp_allocator->freed = true;
	}
}

lip_allocator_t*
lip_temp_allocator_create(lip_allocator_t* allocator)
{
	lip_temp_allocator_t* temp_allocator = lip_new(allocator, lip_temp_allocator_t);
	temp_allocator->allocator = allocator;
	temp_allocator->size = 0;
	temp_allocator->mem = NULL;
	temp_allocator->freed = true;
	temp_allocator->vtable.realloc = lip_temp_allocator_realloc;
	temp_allocator->vtable.free = lip_temp_allocator_free;
	return &temp_allocator->vtable;
}

void
lip_temp_allocator_destroy(lip_allocator_t* allocator)
{
	lip_temp_allocator_t* temp_allocator =
		LIP_CONTAINER_OF(allocator, lip_temp_allocator_t, vtable);
	if(temp_allocator->mem)
	{
		lip_free(temp_allocator->allocator, temp_allocator->mem);
	}
	lip_free(temp_allocator->allocator, temp_allocator);
}
