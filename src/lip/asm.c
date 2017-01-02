#include <lip/asm.h>
#include <lip/vm.h>
#include <lip/array.h>
#include "prim_ops.h"

void
lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator)
{
	lasm->allocator = allocator;
	lasm->labels = lip_array_create(allocator, lip_asm_index_t, 0);
	lasm->jumps = lip_array_create(allocator, lip_asm_index_t, 0);
	lasm->instructions = lip_array_create(allocator, lip_tagged_instruction_t, 0);
	lasm->functions = lip_array_create(allocator, lip_function_t*, 0);
	lasm->imports = lip_array_create(allocator, uint32_t, 0);
	lasm->constants = lip_array_create(allocator, lip_value_t, 0);
	lasm->string_pool = lip_array_create(allocator, lip_string_ref_t, 0);
	lasm->string_layout = lip_array_create(allocator, lip_memblock_info_t, 0);
	lasm->nested_layout = lip_array_create(allocator, lip_memblock_info_t, 0);
	lasm->function_layout = lip_array_create(allocator, lip_memblock_info_t*, 0);
}

void lip_asm_cleanup(lip_asm_t* lasm)
{
	lip_array_destroy(lasm->function_layout);
	lip_array_destroy(lasm->nested_layout);
	lip_array_destroy(lasm->string_layout);
	lip_array_destroy(lasm->string_pool);
	lip_array_destroy(lasm->constants);
	lip_array_destroy(lasm->imports);
	lip_array_destroy(lasm->functions);
	lip_array_destroy(lasm->instructions);
	lip_array_destroy(lasm->jumps);
	lip_array_destroy(lasm->labels);
}

void lip_asm_begin(lip_asm_t* lasm, lip_string_ref_t source_name, lip_loc_range_t location)
{
	lasm->source_name = source_name;
	lasm->location = location;
	lip_array_clear(lasm->labels);
	lip_array_clear(lasm->jumps);
	lip_array_clear(lasm->instructions);
	lip_array_clear(lasm->functions);
	lip_array_clear(lasm->imports);
	lip_array_clear(lasm->constants);
	lip_array_clear(lasm->string_pool);
	lip_array_clear(lasm->string_layout);
	lip_array_clear(lasm->nested_layout);
	lip_array_clear(lasm->function_layout);
}

lip_asm_index_t
lip_asm_new_label(lip_asm_t* lasm)
{
	lip_asm_index_t index = lip_array_len(lasm->labels);
	lip_array_push(lasm->labels, 0);
	return index;
}

lip_asm_index_t
lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function)
{
	lip_asm_index_t index = lip_array_len(lasm->functions);
	lip_array_push(lasm->functions, function);
	lip_array_push(lasm->nested_layout, ((lip_memblock_info_t){
		.element_size = function->size,
		.num_elements = 1,
		.alignment = lip_function_t_alignment
	}));
	return index;
}

static uint32_t
lip_asm_alloc_string(lip_asm_t* lasm, lip_string_ref_t string)
{
	uint32_t num_strings = lip_array_len(lasm->string_pool);
	for(uint32_t i = 0; i < num_strings; ++i)
	{
		if(lip_string_ref_equal(lasm->string_pool[i], string))
		{
			return i;
		}
	}

	uint32_t index = lip_array_len(lasm->string_pool);
	lip_array_push(lasm->string_pool, string);
	lip_array_push(lasm->string_layout, ((lip_memblock_info_t){
		.element_size = sizeof(lip_string_t) + string.length + 1, // 1 = null-terminator
		.num_elements = 1,
		.alignment = lip_string_t_alignment
	}));
	return index;
}

lip_asm_index_t
lip_asm_alloc_import(lip_asm_t* lasm, lip_string_ref_t import)
{
	uint32_t num_imports = lip_array_len(lasm->imports);
	for(uint32_t i = 0; i < num_imports; ++i)
	{
		if(lip_string_ref_equal(lasm->string_pool[lasm->imports[i]], import))
		{
			return i;
		}
	}

	uint32_t index = lip_array_len(lasm->imports);
	lip_array_push(lasm->imports, lip_asm_alloc_string(lasm, import));
	return index;
}

lip_asm_index_t
lip_asm_alloc_numeric_constant(lip_asm_t* lasm, double number)
{
	uint32_t num_constants = lip_array_len(lasm->constants);
	for(uint32_t i = 0; i < num_constants; ++i)
	{
		lip_value_t constant = lasm->constants[i];
		if(constant.type == LIP_VAL_NUMBER && constant.data.number == number)
		{
			return i;
		}
	}

	uint32_t index = lip_array_len(lasm->constants);
	lip_array_push(lasm->constants, ((lip_value_t){
		.type = LIP_VAL_NUMBER,
		.data = {.number = number}
	}));
	return index;
}

static lip_asm_index_t
lip_asm_alloc_string_type_constant(
	lip_asm_t* lasm, lip_string_ref_t string, lip_value_type_t type
)
{
	uint32_t num_constants = lip_array_len(lasm->constants);
	for(uint32_t i = 0; i < num_constants; ++i)
	{
		lip_value_t constant = lasm->constants[i];
		if(true
			&& constant.type == type
			&& lip_string_ref_equal(lasm->string_pool[constant.data.index], string))
		{
			return i;
		}
	}

	uint32_t index = lip_array_len(lasm->constants);
	lip_array_push(lasm->constants, ((lip_value_t){
		.type = type,
		.data = {.index = lip_asm_alloc_string(lasm, string)}
	}));
	return index;
}

lip_asm_index_t
lip_asm_alloc_string_constant(lip_asm_t* lasm, lip_string_ref_t string)
{
	return lip_asm_alloc_string_type_constant(lasm, string, LIP_VAL_STRING);
}

lip_asm_index_t
lip_asm_alloc_symbol(lip_asm_t* lasm, lip_string_ref_t string)
{
	return lip_asm_alloc_string_type_constant(lasm, string, LIP_VAL_SYMBOL);
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
lip_asm_end(lip_asm_t* lasm, lip_allocator_t* allocator)
{
	// Eliminate [NIL; POP 1] sequences where POP 1 is not the last instruction
	{
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
		lip_asm_index_t out_index = 0;
		for(lip_asm_index_t i = 0; i < num_instructions; ++i)
		{
			bool skip = false;
			if(i + 2 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2;
				lip_operand_t operand1, operand2;
				lip_disasm(lasm->instructions[i].instruction, &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1].instruction, &opcode2, &operand2);

				if(true
					&& opcode1 == LIP_OP_NIL
					&& opcode2 == LIP_OP_POP
					&& operand2 == 1)
				{
					skip = true;
				}
			}

			if(skip)
			{
				++i;
			}
			else
			{
				lasm->instructions[out_index] = lasm->instructions[i];
				++out_index;
			}
		}
		lip_array_resize(lasm->instructions, out_index);
	}

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

	// Prim op inlining: Tranfrom [IMP op; CALL y] where op is a prim op into
	// [OP y]
	{
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
		lip_asm_index_t out_index = 0;
		for(lip_asm_index_t i = 0; i < num_instructions; ++i)
		{
			lasm->instructions[out_index] = lasm->instructions[i];

			if(i + 1 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2;
				lip_operand_t operand1, operand2;
				lip_disasm(lasm->instructions[i].instruction, &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1].instruction, &opcode2, &operand2);

				if(opcode1 == LIP_OP_IMP && opcode2 == LIP_OP_CALL)
				{
					lip_string_ref_t symbol = lasm->string_pool[lasm->imports[operand1]];
					lip_instruction_t op_instr = 0;

#define LIP_PRIM_OP_OPTIMIZE(op, name) \
					if(lip_string_ref_equal(lip_string_ref(#op), symbol)) { \
						op_instr = lip_asm(LIP_OP_ ## name, operand2); \
					}
					LIP_PRIM_OP(LIP_PRIM_OP_OPTIMIZE)

					if(op_instr != 0)
					{
						lasm->instructions[out_index].instruction = op_instr;
						++i;
					}
				}
			}

			++out_index;
		}
		lip_array_resize(lasm->instructions, out_index);
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
			lasm->instructions[out_index] = lasm->instructions[i];

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
			}

			++out_index;
		}
		lip_array_resize(lasm->instructions, out_index);
	}

	// Translate jumps
	{
		// Remove all labels and record jump addresses
		lip_asm_index_t num_instructions = lip_array_len(lasm->instructions);
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
					lip_array_push(lasm->jumps, out_index);
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

	size_t num_imports = lip_array_len(lasm->imports);
	size_t num_constants = lip_array_len(lasm->constants);
	size_t num_functions = lip_array_len(lasm->functions);
	size_t num_instructions = lip_array_len(lasm->instructions);

	lip_memblock_info_t header_block = LIP_ARRAY_BLOCK(lip_function_t, 1);
	lip_array_push(lasm->function_layout, &header_block);

	lip_memblock_info_t source_name_block = (lip_memblock_info_t){
		.element_size = sizeof(lip_string_t) + lasm->source_name.length,
		.num_elements = 1,
		.alignment = lip_string_t_alignment
	};
	lip_array_push(lasm->function_layout, &source_name_block);

	lip_memblock_info_t import_block = LIP_ARRAY_BLOCK(lip_import_t, num_imports);
	lip_array_push(lasm->function_layout, &import_block);

	lip_memblock_info_t constant_block = LIP_ARRAY_BLOCK(lip_value_t, num_constants);
	lip_array_push(lasm->function_layout, &constant_block);

	lip_memblock_info_t nested_block = LIP_ARRAY_BLOCK(uint32_t, num_functions);
	lip_array_push(lasm->function_layout, &nested_block);

	lip_memblock_info_t instruction_block = LIP_ARRAY_BLOCK(lip_instruction_t, num_instructions);
	lip_array_push(lasm->function_layout, &instruction_block);

	lip_memblock_info_t location_block = LIP_ARRAY_BLOCK(lip_loc_range_t, num_instructions + 1);
	lip_array_push(lasm->function_layout, &location_block);

	lip_array_foreach(lip_memblock_info_t, block, lasm->string_layout)
	{
		lip_array_push(lasm->function_layout, block);
	}

	lip_array_foreach(lip_memblock_info_t, block, lasm->nested_layout)
	{
		lip_array_push(lasm->function_layout, block);
	}

	lip_memblock_info_t block_info = lip_align_memblocks(
		lip_array_len(lasm->function_layout), lasm->function_layout
	);
	// Only basic scalar types are used so no need to worry about alignment of
	// header
	lip_function_t* function = lip_malloc(allocator, block_info.num_elements);
	memset(function, 0, block_info.num_elements);

	function->size = block_info.num_elements;
	function->num_args = 0;
	function->num_locals = 0;
	function->num_imports = num_imports;
	function->num_constants = num_constants;
	function->num_instructions = num_instructions;
	function->num_functions = num_functions;

	lip_string_t* source_name = lip_locate_memblock(function, &source_name_block);
	source_name->length = lasm->source_name.length;
	memcpy(source_name->ptr, lasm->source_name.ptr, lasm->source_name.length);

	lip_import_t* imports = lip_locate_memblock(function, &import_block);
	for(uint32_t i = 0; i < num_imports; ++i)
	{
		uint32_t import_string_index = lasm->imports[i];
		imports[i].name = lasm->string_layout[import_string_index].offset;
		imports[i].value = (lip_value_t){
			.type = LIP_VAL_PLACEHOLDER,
			.data = { .reference = NULL }
		};
	}

	lip_value_t* constants = lip_locate_memblock(function, &constant_block);
	for(uint32_t i = 0; i < num_constants; ++i)
	{
		lip_value_t constant = lasm->constants[i];
		constants[i].type = constant.type;
		switch(constant.type)
		{
			case LIP_VAL_SYMBOL:
			case LIP_VAL_STRING:
				constants[i].data.index = lasm->string_layout[constant.data.index].offset;
				break;
			case LIP_VAL_NUMBER:
				constants[i].data.number = constant.data.number;
				break;
			default:
				break;
		}
	}

	uint32_t* functions = lip_locate_memblock(function, &nested_block);
	for(uint32_t i = 0; i < num_functions; ++i)
	{
		functions[i] = lasm->nested_layout[i].offset;
	}

	lip_instruction_t* instructions = lip_locate_memblock(function, &instruction_block);
	lip_loc_range_t* locations = lip_locate_memblock(function, &location_block);
	locations[0] = lasm->location;
	for(uint32_t i = 0; i < num_instructions; ++i)
	{
		instructions[i] = lasm->instructions[i].instruction;
		locations[i + 1] = lasm->instructions[i].location;
	}

	size_t num_strings = lip_array_len(lasm->string_pool);
	for(uint32_t i = 0; i < num_strings; ++i)
	{
		lip_string_t* string = lip_locate_memblock(function, &lasm->string_layout[i]);
		string->length = lasm->string_pool[i].length;
		memcpy(string->ptr, lasm->string_pool[i].ptr, lasm->string_pool[i].length);
		string->ptr[string->length] = '\0';
	}

	for(uint32_t i = 0; i < num_functions; ++i)
	{
		lip_function_t* nested_function = lip_locate_memblock(function, &lasm->nested_layout[i]);
		memcpy(nested_function, lasm->functions[i], lasm->functions[i]->size);
	}

	return function;
}
