#include "stack_allocator.h"
#include <string.h>

typedef struct stack_allocator_t
{
	lip_allocator_t vtable;
	lip_allocator_t* backing_allocator;
	size_t index;
	size_t max;
	char stack[];
} stack_allocator_t;

typedef struct mem_block
{
	size_t size;
	char mem[];
} mem_block;

static void* stack_alloc(void* self, size_t size)
{
	stack_allocator_t* allocator = self;
	size_t bump = (size + (sizeof(void*) - 1)) / sizeof(void*) * sizeof(void*);
	if(allocator->max - allocator->index >= bump)
	{
		void* mem = allocator->stack + allocator->index;
		allocator->index += bump;
		return mem;
	}
	else
	{
		return NULL;
	}
}

static void* stack_realloc(void* self, void* old, size_t size)
{
	mem_block* block = stack_alloc(self, sizeof(mem_block) + size);
	if(block == NULL) { return NULL; }
	block->size = size;
	if(old != NULL)
	{
		mem_block* old_block = (mem_block*)((char*)old - offsetof(mem_block, mem));
		memcpy(block->mem, old_block->mem, old_block->size);
	}
	return block->mem;
}

static void stack_free(void* self, void* ptr)
{
	(void)self;
	(void)ptr;
}

lip_allocator_t* lip_stack_allocator_new(
	lip_allocator_t* allocator, size_t heap_size
)
{
	size_t allocator_size = sizeof(stack_allocator_t) + heap_size;
	stack_allocator_t* stack_allocator = lip_malloc(allocator, allocator_size);
	stack_allocator->backing_allocator = allocator;
	stack_allocator->index = 0;
	stack_allocator->max = heap_size;
	stack_allocator->vtable.realloc = stack_realloc;
	stack_allocator->vtable.free = stack_free;
	return &stack_allocator->vtable;
}

void lip_stack_allocator_delete(lip_allocator_t* allocator)
{
	stack_allocator_t* stack_allocator = (stack_allocator_t*)allocator;
	lip_free(stack_allocator->backing_allocator, stack_allocator);
}

void lip_stack_allocator_reset(lip_allocator_t* allocator)
{
	stack_allocator_t* stack_allocator = (stack_allocator_t*)allocator;
	stack_allocator->index = 0;
}
