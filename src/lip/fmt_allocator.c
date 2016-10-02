#include "ex/fmt_allocator.h"

static void*
lip_fmt_allocator_realloc(lip_allocator_t* vtable, void* old, size_t size)
{
	lip_fmt_allocator_t* fmt_allocator =
		LIP_CONTAINER_OF(vtable, lip_fmt_allocator_t, vtable);

	if(!(false
		|| (old != NULL && old == fmt_allocator->mem) // realloc
		|| (old == NULL && fmt_allocator->freed))) // malloc
	{
		return NULL;
	}

	fmt_allocator->freed = false;
	void* mem = NULL;
	if(fmt_allocator->size < size)
	{
		mem = lip_realloc(fmt_allocator->allocator, fmt_allocator->mem, size);
		fmt_allocator->mem = mem;
		fmt_allocator->size = size;
	}
	else
	{
		mem = fmt_allocator->mem;
	}

	return mem;
}

static void
lip_fmt_allocator_free(lip_allocator_t* vtable, void* mem)
{
	lip_fmt_allocator_t* fmt_allocator =
		LIP_CONTAINER_OF(vtable, lip_fmt_allocator_t, vtable);
	if(mem == fmt_allocator->mem)
	{
		fmt_allocator->freed = true;
	}
}

lip_allocator_t*
lip_fmt_allocator_create(lip_allocator_t* allocator)
{
	lip_fmt_allocator_t* fmt_allocator = lip_new(allocator, lip_fmt_allocator_t);
	fmt_allocator->allocator = allocator;
	fmt_allocator->size = 0;
	fmt_allocator->mem = NULL;
	fmt_allocator->freed = true;
	fmt_allocator->vtable.realloc = lip_fmt_allocator_realloc;
	fmt_allocator->vtable.free = lip_fmt_allocator_free;
	return &fmt_allocator->vtable;
}

void
lip_fmt_allocator_destroy(lip_allocator_t* allocator)
{
	lip_fmt_allocator_t* fmt_allocator =
		LIP_CONTAINER_OF(allocator, lip_fmt_allocator_t, vtable);
	if(fmt_allocator->mem)
	{
		lip_free(fmt_allocator->allocator, fmt_allocator->mem);
	}
	lip_free(fmt_allocator->allocator, fmt_allocator);
}
