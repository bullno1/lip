#include <stdlib.h>
#include "memory.h"

static void*
lip_crt_realloc(void* context, void* old, size_t size)
{
	(void)context;
	return realloc(old, size);
}

static void
lip_crt_free(void* context, void* mem)
{
	(void)context;
	free(mem);
}

lip_memblock_info_t
lip_align_memblocks(unsigned int num_blocks, lip_memblock_info_t** blocks)
{
	lip_memblock_info_t result = {
		.element_size = sizeof(char),
		.alignment = 0,
		.num_elements = 0,
		.offset = 0
	};

	// Find max alignment
	for(unsigned int i = 0; i < num_blocks; ++i)
	{
		result.alignment = LIP_MAX(result.alignment, blocks[i]->alignment);
	}

	// Assume base addressed is aligned to the max alignment, place blocks
	for(unsigned int i = 0; i < num_blocks; ++i)
	{
		lip_memblock_info_t* block = blocks[i];

		size_t rem = result.num_elements % result.alignment;
		size_t shift = result.alignment - rem;
		result.num_elements += rem == 0 ? 0 : shift;
		block->offset = result.num_elements;

		result.num_elements += block->num_elements * block->element_size;
	}

	return result;
}

static lip_allocator_t lip_crt_allocator = {
	.realloc = lip_crt_realloc,
	.free = lip_crt_free
};

lip_allocator_t* lip_default_allocator = &lip_crt_allocator;
