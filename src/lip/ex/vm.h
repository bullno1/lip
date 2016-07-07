#ifndef LIP_VM_EX_H
#define LIP_VM_EX_H

#include "../vm.h"
#include "../opcode.h"

typedef struct lip_stack_frame_s lip_stack_frame_t;
typedef struct lip_symtab_entry_s lip_symtab_entry_t;
typedef struct lip_import_entry_s lip_import_entry_t;

struct lip_function_s
{
	uint16_t num_args;
	uint16_t num_locals;

	uint32_t num_instructions;
	uint32_t num_functions;

	lip_instruction_t* instructions;
	lip_function_t** functions;
	lip_loc_range_t* locations;

	// Layout:
	//
	// [lip_function_s]: header
	// [lip_function_t*...]: nested functions
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

size_t
lip_vm_memory_required(lip_vm_config_t* config);

void
lip_vm_init(lip_vm_t* vm, lip_vm_config_t* config, void* mem);

#endif
