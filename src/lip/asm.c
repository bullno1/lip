#include "ex/asm.h"
#include "ex/vm.h"
#include "utils.h"
#include "array.h"
#include "io.h"

LIP_IMPLEMENT_CONSTRUCTOR_AND_DESTRUCTOR(lip_asm)

void
lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator)
{
	lasm->allocator = allocator;
	lasm->labels = lip_array_create(allocator, lip_asm_index_t, 0);
	lasm->jumps = lip_array_create(allocator, lip_asm_index_t, 0);
	lasm->instructions = lip_array_create(allocator, lip_tagged_instruction_t, 0);
	lasm->functions = lip_array_create(allocator, lip_function_t*, 0);
}

void lip_asm_cleanup(lip_asm_t* lasm)
{
	lip_array_destroy(lasm->labels);
	lip_array_destroy(lasm->jumps);
	lip_array_destroy(lasm->instructions);
	lip_array_destroy(lasm->functions);
}

void lip_asm_begin(lip_asm_t* lasm)
{
	lip_array_clear(lasm->labels);
	lip_array_clear(lasm->jumps);
	lip_array_clear(lasm->instructions);
	lip_array_clear(lasm->functions);
	lasm->num_locals = 0;
}

lip_asm_index_t
lip_asm_new_label(lip_asm_t* lasm)
{
	lip_asm_index_t index = lip_array_len(lasm->labels);
	lip_array_push(lasm->labels, 0);
	return index;
}

lip_asm_index_t
lip_asm_new_local(lip_asm_t* lasm)
{
	return ++lasm->num_locals;
}

lip_asm_index_t
lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function)
{
	lip_asm_index_t index = lip_array_len(lasm->functions);
	lip_array_push(lasm->functions, function);
	return index;
}

void
lip_asm_add(
	lip_asm_t* lasm,
	lip_opcode_t opcode,
	lip_operand_t operand,
	lip_loc_range_t location
)
{
	lip_array_push(lasm->instructions, ((lip_tagged_instruction_t){
		.instruction = lip_asm(opcode, operand),
		.location = location
	}));
}

lip_function_t*
lip_asm_end(lip_asm_t* lasm)
{
	// Transform [JMP l] where l points to RET into [RET]
	{
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
		for(lip_asm_index_t i = 0; i < num_instructions; ++i)
		{
			lip_opcode_t opcode;
			lip_operand_t operand;
			lip_disasm(lasm->instructions[i].instruction, &opcode, &operand);

			if(opcode == LIP_OP_JMP)
			{
				lip_instruction_t label = lip_asm(LIP_OP_LABEL, operand);
				for(lip_asm_index_t j = 0; j < num_instructions; ++j)
				{
					if(true
						&& lasm->instructions[j].instruction == label
						&& j + 1 < num_instructions)
					{
						lip_opcode_t target_opcode;
						lip_operand_t target_operand;
						lip_disasm(
							lasm->instructions[j + 1].instruction,
							&target_opcode,
							&target_operand
						);

						if(target_opcode == LIP_OP_RET)
						{
							lasm->instructions[i].instruction = lip_asm(LIP_OP_RET, 0);
						}
					}
				}
			}
		}
	}

	// Perform Tail call optimization
	{
		// Transform [CALL n; LABEL l; RET] into [TAIL n; LABEL l; RET]
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
		for(lip_asm_index_t i = 0; i < num_instructions; ++i)
		{
			if(i + 2 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2, opcode3;
				lip_operand_t operand1, operand2, operand3;
				lip_disasm(lasm->instructions[i].instruction, &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1].instruction, &opcode2, &operand2);
				lip_disasm(lasm->instructions[i + 2].instruction, &opcode3, &operand3);

				if(true &&
					opcode1 == LIP_OP_CALL
					&& (uint32_t)opcode2 == LIP_OP_LABEL
					&& opcode3 == LIP_OP_RET)
				{
					lasm->instructions[i].instruction = lip_asm(LIP_OP_TAIL, operand1);
				}
			}
		}

		// Transform [CALL n; RET] into [TAIL n]
		lip_asm_index_t out_index = 0;
		for(lip_asm_index_t i = 0; i < num_instructions; ++i)
		{
			if(i + 1 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2;
				lip_operand_t operand1, operand2;
				lip_disasm(lasm->instructions[i].instruction, &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1].instruction, &opcode2, &operand2);

				if(opcode1 == LIP_OP_CALL && opcode2 == LIP_OP_RET)
				{
					lip_instruction_t tailcall = lip_asm(LIP_OP_TAIL, operand1);
					lasm->instructions[out_index].instruction = tailcall;
					++i;
				}
				else
				{
					lasm->instructions[out_index] = lasm->instructions[i];
				}
			}
			else
			{
				lasm->instructions[out_index] = lasm->instructions[i];
			}

			++out_index;
		}
		lip_array_resize(lasm->instructions, out_index);
	}

	// Translate jumps
	{
		// Remove all labels and record jump addresses
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
		lip_array_clear(lasm->jumps);
		lip_asm_index_t out_index = 0;
		for(lip_asm_index_t index = 0; index < num_instructions; ++index)
		{
			lip_opcode_t opcode;
			lip_operand_t operand;
			lip_disasm(lasm->instructions[index].instruction, &opcode, &operand);

			switch((uint32_t)opcode)
			{
				case LIP_OP_LABEL:
					lasm->labels[operand] = out_index;
					break;
				case LIP_OP_JMP:
				case LIP_OP_JOF:
					lip_array_push(lasm->jumps, index);
					break;
			}

			// Overwrite a label with the next instruction if needed
			lasm->instructions[out_index] = lasm->instructions[index];
			if((uint32_t)opcode != LIP_OP_LABEL) { ++out_index; }
		}
		lip_array_resize(lasm->instructions, out_index);

		// Replace all jumps with label address
		lip_array_foreach(lip_asm_index_t, itr, lasm->jumps)
		{
			lip_asm_index_t address = *itr;

			lip_opcode_t opcode;
			lip_operand_t operand;
			lip_disasm(lasm->instructions[address].instruction, &opcode, &operand);
			lip_instruction_t jump = lip_asm(opcode, lasm->labels[operand]);
			lasm->instructions[address].instruction = jump;
		}
	}

	size_t num_functions = lip_array_len(lasm->functions);
	size_t num_instructions = lip_array_len(lasm->instructions);

	lip_memblock_info_t layout[] = {
		LIP_ARRAY_BLOCK(lip_function_t*, num_functions),
		LIP_ARRAY_BLOCK(lip_instruction_t, num_instructions),
		LIP_ARRAY_BLOCK(lip_loc_range_t, num_instructions)
	};
	size_t num_blocks = LIP_STATIC_ARRAY_LEN(layout);
	lip_memblock_info_t block_info = lip_align_memblocks(num_blocks, layout);
	size_t block_size = 0
		+ sizeof(lip_function_t)
		+ block_info.alignment - 1
		+ block_info.num_elements;
	char* ptr = lip_malloc(lasm->allocator, block_size);

	lip_function_t* function = (lip_function_t*)ptr;
	function->num_locals = lasm->num_locals;
	function->num_instructions = num_instructions;
	function->num_functions = num_functions;
	ptr += sizeof(lip_function_t);

	ptr = lip_align_ptr(ptr, block_info.alignment);

	lip_function_t** functions = lip_locate_memblock(ptr, &layout[0]);
	memcpy(functions, lasm->functions, num_functions * sizeof(lip_function_t*));

	lip_instruction_t* instructions = lip_locate_memblock(ptr, &layout[1]);
	lip_loc_range_t* locations = lip_locate_memblock(ptr, &layout[2]);
	for(size_t i = 0; i < num_instructions; ++i)
	{
		instructions[i] = lasm->instructions[i].instruction;
		locations[i] = lasm->instructions[i].location;
	}

	function->instructions = instructions;
	function->functions = functions;
	function->locations = locations;

	return function;
}

lip_memblock_info_t
lip_function_layout(lip_function_t* function)
{
	lip_memblock_info_t layout[] = {
		LIP_ARRAY_BLOCK(lip_function_t*, function->num_functions),
		LIP_ARRAY_BLOCK(lip_instruction_t, function->num_instructions),
		LIP_ARRAY_BLOCK(lip_loc_range_t, function->num_instructions)
	};
	size_t num_blocks = LIP_STATIC_ARRAY_LEN(layout);
	lip_memblock_info_t block_info = lip_align_memblocks(num_blocks, layout);
	size_t block_size = 0
		+ sizeof(lip_function_t)
		+ block_info.alignment - 1
		+ block_info.num_elements;

	return (lip_memblock_info_t) {
		.element_size = 1,
		.num_elements = block_size,
		.alignment = LIP_ALIGN_OF(lip_function_t)
	};
}
