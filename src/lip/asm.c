#include "asm.h"
#include <stdarg.h>
#include <string.h>
#include "array.h"
#include "allocator.h"
#include <stdio.h>

lip_instruction_t lip_asm(lip_opcode_t opcode, int32_t operand)
{
	return ((opcode & 0xFF) << 24) | (operand & 0x00FFFFFF);
}

void lip_disasm(
	lip_instruction_t instruction, lip_opcode_t* opcode, int32_t* operand
)
{
	*opcode = (instruction >> 24) & 0xFF;
	*operand = instruction & 0x00FFFFFF;
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
}

void lip_asm_begin(lip_asm_t* lasm)
{
	lip_array_resize(lasm->labels, 0);
	lip_array_resize(lasm->instructions, 0);
	lip_array_resize(lasm->constants, 0);
	lip_array_resize(lasm->functions, 0);
	lip_array_resize(lasm->import_symbols, 0);
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
	return lasm->num_locals++;
}

lip_asm_index_t lip_asm_new_constant(lip_asm_t* lasm, lip_value_t* value)
{
	//TODO: assert that constants are only of primitive types or strings
	lip_asm_index_t index = lip_array_len(lasm->constants);
	lip_array_push(lasm->constants, *value);
	return index;
}

lip_asm_index_t lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function)
{
	lip_asm_index_t index = lip_array_len(lasm->functions);
	lip_array_push(lasm->functions, *function);
	return index;
}

lip_asm_index_t lip_asm_new_import(
	lip_asm_t* lasm, const char* symbol, size_t length
)
{
	lip_asm_index_t index = lip_array_len(lasm->import_symbols);
	lip_string_ref_t asm_symbol = { length , symbol };
	lip_array_push(lasm->import_symbols, asm_symbol);
	return index;
}

lip_function_t* lip_asm_end(lip_asm_t* lasm)
{
	//TODO: build a string table for constant strings

	// Remove all labels, recording their addresses and record jump addresses
	size_t num_instructions = lip_array_len(lasm->instructions);
	lip_array_resize(lasm->jumps, 0);
	for(lip_asm_index_t index = 0; index < num_instructions; ++index)
	{
		lip_opcode_t opcode;
		int32_t operand;
		lip_disasm(lasm->instructions[index], &opcode, &operand);

		switch((uint32_t)opcode)
		{
			case LIP_OP_LABEL:
				memmove(
					&lasm->instructions[index],
					&lasm->instructions[index + 1],
					(num_instructions - index - 1) * sizeof(lip_instruction_t)
				);
				lasm->labels[operand] = index;
				--num_instructions;
				break;
			case LIP_OP_JMP:
			case LIP_OP_JOF:
				lip_array_push(lasm->jumps, index);
				break;
		}
	}
	lip_array_resize(lasm->instructions, num_instructions);

	// Replace all jumps with label address
	lip_array_foreach(lip_asm_index_t, itr, lasm->jumps)
	{
		lip_asm_index_t address = *itr;

		lip_opcode_t opcode;
		int32_t operand;
		lip_disasm(lasm->instructions[address], &opcode, &operand);
		lasm->instructions[address] = lip_asm(opcode, lasm->labels[operand]);
	}

	// Calculate the size of the memory block needed for the function
	size_t header_size = sizeof(lip_function_t);
	size_t code_size = num_instructions * sizeof(lip_instruction_t);
	size_t const_pool_size =
		lip_array_len(lasm->constants) * sizeof(lip_value_t);
	size_t nested_function_table_size =
		lip_array_len(lasm->functions) * sizeof(lip_function_t*);

	size_t num_imports = lip_array_len(lasm->import_symbols);
	size_t import_table_size = num_imports * sizeof(lip_string_t*);
	size_t symbol_section_size = 0;
	lip_array_foreach(lip_string_ref_t, itr, lasm->import_symbols)
	{
		size_t symbol_size = sizeof(lip_string_t) + itr->length;
		// align to void* size
		symbol_section_size +=
			(symbol_size + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
	}

	size_t import_index_size = num_imports * sizeof(uint32_t);
	size_t block_size =
		  header_size
		+ code_size
		+ const_pool_size
		+ nested_function_table_size
		+ import_table_size
		+ symbol_section_size
		+ import_index_size;

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
	function->functions = (lip_function_t*)ptr;
	memcpy(ptr, lasm->functions, nested_function_table_size);
	ptr += nested_function_table_size;

	// Reserve space for index table
	function->import_indices = (uint32_t*)ptr;
	memset(ptr, 0, import_index_size);
	ptr += import_index_size;

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

		size_t symbol_size = sizeof(lip_string_t) + symbol->length;
		ptr +=
			(symbol_size + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
	}

	return function;
}

void lip_asm_cleanup(lip_asm_t* lasm)
{
	lip_array_delete(lasm->import_symbols);
	lip_array_delete(lasm->functions);
	lip_array_delete(lasm->constants);
	lip_array_delete(lasm->instructions);
	lip_array_delete(lasm->jumps);
	lip_array_delete(lasm->labels);
}
