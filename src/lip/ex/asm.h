#ifndef LIP_ASM_EX_H
#define LIP_ASM_EX_H

#include "../asm.h"
#include "../memory.h"

typedef struct lip_tagged_instruction_s lip_tagged_instruction_t;

struct lip_tagged_instruction_s
{
	lip_instruction_t instruction;
	lip_loc_range_t location;
};

struct lip_asm_s
{
	lip_allocator_t* allocator;
	lip_asm_index_t num_locals;
	lip_array(lip_asm_index_t) labels;
	lip_array(lip_asm_index_t) jumps;
	lip_array(lip_tagged_instruction_t) instructions;
	lip_array(lip_function_t*) functions;
	lip_array(lip_memblock_info_t) nested_layout;
	lip_array(lip_memblock_info_t*) function_layout;
};

void
lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator);

void
lip_asm_cleanup(lip_asm_t* lasm);

#endif
