#ifndef LIP_VM_EX_H
#define LIP_VM_EX_H

#include "../vm.h"
#include "../opcode.h"
#include "../memory.h"

typedef struct lip_stack_frame_s lip_stack_frame_t;
typedef struct lip_symtab_entry_s lip_symtab_entry_t;
typedef struct lip_import_entry_s lip_import_entry_t;

struct lip_function_s
{
	size_t size;

	uint16_t num_args;
	uint16_t num_locals;

	uint32_t num_instructions;
	uint32_t num_functions;

	// Layout:
	//
	// [lip_function_s]: header
	// [uint32_t...]: nested function offsets
	// [lip_instruction_t...]: instructions
	// [lip_loc_range_t...]: source locations
};

struct lip_closure_s
{
	lip_module_t* module;
	union
	{
		lip_function_t* lip;
		struct
		{
			lip_native_fn_t ptr;
			uint16_t num_args;
		} native;
	} function;
	bool is_native;

	uint16_t num_captures;
	lip_value_t environment[];
};

struct lip_stack_frame_s
{
	lip_closure_t* closure;
	lip_instruction_t* pc;
	lip_value_t* ep;
	bool is_native;
};

struct lip_vm_s
{
	lip_vm_config_t config;

	lip_value_t* min_sp;
	lip_value_t* max_sp;
	lip_value_t* min_ep;
	lip_value_t* max_ep;
	lip_stack_frame_t* min_fp;
	lip_stack_frame_t* max_fp;

	lip_value_t* sp;
	lip_stack_frame_t* fp;

	lip_vm_hook_t* hook;
};

static const size_t lip_function_t_alignment =
	LIP_MAX(
		LIP_ALIGN_OF(lip_function_t),
		LIP_MAX(
			LIP_ALIGN_OF(uint32_t),
			LIP_MAX(
				LIP_ALIGN_OF(lip_instruction_t),
				LIP_ALIGN_OF(lip_loc_range_t))));

size_t
lip_vm_memory_required(lip_vm_config_t* config);

void
lip_vm_init(lip_vm_t* vm, lip_vm_config_t* config, void* mem);

LIP_MAYBE_UNUSED static inline uint32_t*
lip_function_get_nested_table(lip_function_t* function)
{
	lip_function_t* function_end = function + 1;
	return lip_align_ptr(function_end, LIP_ALIGN_OF(uint32_t));
}

LIP_MAYBE_UNUSED static inline lip_instruction_t*
lip_function_get_instructions(lip_function_t* function)
{
	uint32_t* nested_table = lip_function_get_nested_table(function);
	uint32_t* nested_table_end = nested_table + function->num_functions;
	return lip_align_ptr(nested_table_end, LIP_ALIGN_OF(lip_instruction_t));
}

LIP_MAYBE_UNUSED static inline lip_loc_range_t*
lip_function_get_locations(lip_function_t* function)
{
	lip_instruction_t* instructions = lip_function_get_instructions(function);
	lip_instruction_t* instructions_end = instructions + function->num_instructions;
	return lip_align_ptr(instructions_end, LIP_ALIGN_OF(lip_instruction_t));
}

LIP_MAYBE_UNUSED static inline lip_function_t*
lip_function_get_nested_function(lip_function_t* function, uint32_t index)
{
	uint32_t* nested_table = lip_function_get_nested_table(function);
	uint32_t offset = nested_table[index];
	return (lip_function_t*)((char*)function + offset);
}

#endif
