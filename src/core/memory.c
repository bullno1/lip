#include <lip/core/memory.h>

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

		size_t rem = result.num_elements % block->alignment;
		size_t shift = result.alignment - rem;
		result.num_elements += rem == 0 ? 0 : shift;
		block->offset = result.num_elements;

		result.num_elements += block->num_elements * block->element_size;
	}

	return result;
}
