#include "asm.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "array.h"
#include "allocator.h"

lip_instruction_t lip_asm(lip_opcode_t opcode, int32_t operand)
{
	return ((opcode & 0xFF) << 24) | (operand & 0x00FFFFFF);
}

void lip_disasm(
	lip_instruction_t instruction, lip_opcode_t* opcode, int32_t* operand
)
{
	*opcode = (instruction >> 24) & 0xFF;
	*operand = (instruction << 8) >> 8;
}

void lip_asm_print(lip_instruction_t instruction)
{
	lip_opcode_t opcode;
	int32_t operand;
	lip_disasm(instruction, &opcode, &operand);
	int32_t hi_operand = (operand >> 12) & 0xFFF;
	int32_t lo_operand = operand & 0xFFF;
	switch(opcode)
	{
		case LIP_OP_NOP:
		case LIP_OP_NIL:
		case LIP_OP_RET:
		case LIP_OP_LT:
			printf(
				"%-9s (0x%08x)",
				lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
				instruction
			);
			break;
		case LIP_OP_CLS:
			printf(
				"%-4s %d, %d (0x%08x)",
				lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
				hi_operand, lo_operand,
				instruction
			);
			break;
		default:
			printf(
				"%-4s %4d (0x%08x)",
				lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
				operand,
				instruction
			);
			break;
	}
}

void lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator)
{
	lasm->allocator = allocator;
	lasm->labels = lip_array_new(allocator);
	lasm->jumps = lip_array_new(allocator);
	lasm->instructions = lip_array_new(allocator);
	lasm->constants = lip_array_new(allocator);
	lasm->functions = lip_array_new(allocator);
	lasm->import_symbols = lip_array_new(allocator);
	lasm->string_pool = lip_array_new(allocator);
}

void lip_asm_begin(lip_asm_t* lasm)
{
	lip_array_clear(lasm->labels);
	lip_array_clear(lasm->instructions);
	lip_array_clear(lasm->constants);
	lip_array_clear(lasm->functions);
	lip_array_clear(lasm->import_symbols);
	lip_array_clear(lasm->string_pool);
	lasm->num_locals = 0;
}

void lip_asm_add(lip_asm_t* lasm, ...)
{
	va_list args;
	va_start(args, lasm);
	while(true)
	{
		lip_opcode_t opcode = va_arg(args, lip_opcode_t);
		if(opcode == LIP_ASM_END) { break; }

		int32_t operand = va_arg(args, int32_t);
		lip_array_push(lasm->instructions, lip_asm(opcode, operand));
	}
	va_end(args);
}

lip_asm_index_t lip_asm_new_label(lip_asm_t* lasm)
{
	lip_asm_index_t index = lip_array_len(lasm->labels);
	lip_array_push(lasm->labels, 0);
	return index;
}

lip_asm_index_t lip_asm_new_local(lip_asm_t* lasm)
{
	return ++lasm->num_locals;
}

lip_asm_index_t lip_asm_new_number_const(lip_asm_t* lasm, double number)
{
	lip_asm_index_t index = lip_array_len(lasm->constants);
	lip_value_t value = {
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	};
	lip_array_push(lasm->constants, value);
	return index;
}

lip_asm_index_t lip_asm_new_string_const(lip_asm_t* lasm, lip_string_ref_t str)
{
	lip_asm_index_t index = lip_array_len(lasm->constants);

	size_t string_pool_size = lip_array_len(lasm->string_pool);
	lip_value_t value = {
		.type = LIP_VAL_STRING,
		.data = { .reference = (void*)string_pool_size }
	};
	lip_array_push(lasm->constants, value);

	lip_array_resize(
		lasm->string_pool,
		string_pool_size + lip_string_align(str.length)
	);
	lip_string_t* entry = (lip_string_t*)(lasm->string_pool + string_pool_size);
	entry->length = str.length;
	memcpy(entry->ptr, str.ptr, str.length);

	return index;
}

lip_asm_index_t lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function)
{
	lip_asm_index_t index = lip_array_len(lasm->functions);
	lip_array_push(lasm->functions, function);
	return index;
}

lip_asm_index_t lip_asm_new_import(lip_asm_t* lasm, lip_string_ref_t symbol)
{
	lip_asm_index_t index = lip_array_len(lasm->import_symbols);
	lip_array_push(lasm->import_symbols, symbol);
	return index;
}

lip_function_t* lip_asm_end(lip_asm_t* lasm)
{
	// Transform [JMP l] where l points to RET into [RET]
	// Technically, the target instruction could be anything but it only makes
	// sense with RET and we try to be conservative
	{
		unsigned int num_instructions = lip_array_len(lasm->instructions);
		for(unsigned int i = 0; i < num_instructions; ++i)
		{
			lip_opcode_t opcode;
			int32_t operand;
			lip_disasm(lasm->instructions[i], &opcode, &operand);

			if(opcode == LIP_OP_JMP)
			{
				lip_instruction_t label = lip_asm(LIP_OP_LABEL, operand);
				for(unsigned int j = 0; j < num_instructions; ++j)
				{
					if(lasm->instructions[j] == label
					&& j + 1 < num_instructions)
					{
						lip_opcode_t target_opcode;
						int32_t target_operand;
						lip_disasm(
							lasm->instructions[j + 1],
							&target_opcode,
							&target_operand
						);

						if(target_opcode == LIP_OP_RET)
						{
							lasm->instructions[i] = lip_asm(LIP_OP_RET, 0);
						}
					}
				}
			}
		}
	}
	// Perform Tail call optimization
	{
		// Transform [CALL n; LABEL l; RET] into [TAIL n; LABEL l; RET]
		unsigned int num_instructions = lip_array_len(lasm->instructions);
		for(unsigned int i = 0; i < num_instructions; ++i)
		{
			if(i + 2 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2, opcode3;
				int32_t operand1, operand2, operand3;
				lip_disasm(lasm->instructions[i], &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1], &opcode2, &operand2);
				lip_disasm(lasm->instructions[i + 2], &opcode3, &operand3);

				if(opcode1 == LIP_OP_CALL
				&& opcode2 == LIP_OP_LABEL
				&& opcode3 == LIP_OP_RET)
				{
					lasm->instructions[i] = lip_asm(LIP_OP_TAIL, operand1);
				}
			}
		}

		// Transform [CALL n; RET] into [TAIL n]
		unsigned int out_index = 0;
		for(unsigned int i = 0; i < num_instructions; ++i)
		{
			if(i + 1 < num_instructions)
			{
				lip_opcode_t opcode1, opcode2;
				int32_t operand1, operand2;
				lip_disasm(lasm->instructions[i], &opcode1, &operand1);
				lip_disasm(lasm->instructions[i + 1], &opcode2, &operand2);

				if(opcode1 == LIP_OP_CALL && opcode2 == LIP_OP_RET)
				{
					lip_instruction_t tailcall = lip_asm(LIP_OP_TAIL, operand1);
					lasm->instructions[out_index] = tailcall;
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

	{
		// Remove all labels and record jump addresses
		unsigned int num_instructions = lip_array_len(lasm->instructions);
		lip_array_clear(lasm->jumps);
		lip_asm_index_t out_index = 0;
		for(lip_asm_index_t index = 0; index < num_instructions; ++index)
		{
			lip_opcode_t opcode;
			int32_t operand;
			lip_disasm(lasm->instructions[index], &opcode, &operand);

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
			if(opcode != LIP_OP_LABEL) { ++out_index; }
		}
		lip_array_resize(lasm->instructions, out_index);

		// Replace all jumps with label address
		lip_array_foreach(lip_asm_index_t, itr, lasm->jumps)
		{
			lip_asm_index_t address = *itr;

			lip_opcode_t opcode;
			int32_t operand;
			lip_disasm(lasm->instructions[address], &opcode, &operand);
			lasm->instructions[address] = lip_asm(opcode, lasm->labels[operand]);
		}
	}

	// Calculate the size of the memory block needed for the function
	unsigned int num_instructions = lip_array_len(lasm->instructions);
	size_t header_size = sizeof(lip_function_t);
	size_t code_size = num_instructions * sizeof(lip_instruction_t);
	size_t const_pool_size =
		lip_array_len(lasm->constants) * sizeof(lip_value_t);
	size_t nested_function_table_size =
		lip_array_len(lasm->functions) * sizeof(lip_function_t*);
	size_t string_pool_size = lip_array_len(lasm->string_pool);

	size_t num_imports = lip_array_len(lasm->import_symbols);
	size_t import_table_size = num_imports * sizeof(lip_string_t*);
	size_t symbol_section_size = 0;
	lip_array_foreach(lip_string_ref_t, itr, lasm->import_symbols)
	{
		symbol_section_size += lip_string_align(itr->length);
	}

	size_t import_value_table_size = num_imports * sizeof(lip_value_t);
	size_t block_size =
		  header_size
		+ code_size
		+ const_pool_size
		+ nested_function_table_size
		+ string_pool_size
		+ import_table_size
		+ symbol_section_size
		+ import_value_table_size;

	lip_function_t* function = lip_malloc(lasm->allocator, block_size);

	// Write header
	function->arity = 0; // Must be set manually
	function->stack_size = lasm->num_locals;
	function->num_instructions = lip_array_len(lasm->instructions);
	function->num_constants = lip_array_len(lasm->constants);
	function->num_functions = lip_array_len(lasm->functions);
	function->num_imports = num_imports;
	char* ptr = (char*)function + header_size;

	// Write code
	function->instructions = (lip_instruction_t*)ptr;
	memcpy(ptr, lasm->instructions, code_size);
	ptr += code_size;

	// Write constants
	function->constants = (lip_value_t*)ptr;
	memcpy(ptr, lasm->constants, const_pool_size);
	ptr += const_pool_size;

	// Write nested function table
	function->functions = (lip_function_t**)ptr;
	memcpy(ptr, lasm->functions, nested_function_table_size);
	ptr += nested_function_table_size;

	// Reserve space for value table
	function->import_values = (lip_value_t*)ptr;
	memset(ptr, 0, import_value_table_size);
	ptr += import_value_table_size;

	// Write import table
	function->import_symbols = (lip_string_t**)ptr;
	ptr += import_table_size;

	// Write symbol section
	for(int i = 0; i < lip_array_len(lasm->import_symbols); ++i)
	{
		lip_string_ref_t* symbol = lasm->import_symbols + i;
		lip_string_t* entry = (lip_string_t*)ptr;
		entry->length = symbol->length;
		memcpy(&entry->ptr, symbol->ptr, symbol->length);
		function->import_symbols[i] = (lip_string_t*)ptr;

		ptr += lip_string_align(symbol->length);
	}

	// Write string pool
	memcpy(ptr, lasm->string_pool, string_pool_size);
	// Fix pointers in constant table
	for(int i = 0; i < function->num_constants; ++i)
	{
		lip_value_t* constant = &function->constants[i];
		if(constant->type == LIP_VAL_STRING)
		{
			constant->data.reference = ptr + (uintptr_t)constant->data.reference;
		}
	}
	ptr += string_pool_size;

	return function;
}

void lip_asm_cleanup(lip_asm_t* lasm)
{
	lip_array_delete(lasm->string_pool);
	lip_array_delete(lasm->import_symbols);
	lip_array_delete(lasm->functions);
	lip_array_delete(lasm->constants);
	lip_array_delete(lasm->instructions);
	lip_array_delete(lasm->jumps);
	lip_array_delete(lasm->labels);
}
