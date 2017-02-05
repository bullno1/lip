#ifndef LIP_VM_H
#define LIP_VM_H

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4116)
#endif

#include <stdarg.h>
#include "extra.h"
#include "opcode.h"
#include "memory.h"

typedef struct lip_stack_frame_s lip_stack_frame_t;
typedef struct lip_function_layout_s lip_function_layout_t;
typedef struct lip_import_s lip_import_t;
typedef struct lip_runtime_interface_s lip_runtime_interface_t;

struct lip_runtime_interface_s
{
	bool(*resolve_import)(
		lip_runtime_interface_t* rt,
		lip_string_t* symbol_name,
		lip_value_t* result
	);
	void*(*malloc)(lip_runtime_interface_t* rt, lip_value_type_t type, size_t size);
	const char*(*format)(lip_runtime_interface_t* rt, const char* fmt, va_list args);
};

struct lip_import_s
{
	uint32_t name;
	lip_value_t value;
};

/**
 * Layout:
 *
 * [lip_function_t]: header
 * [lip_string_t]: source name
 * [lip_import_t...]: imports
 * [lip_value_t...]: constant pool, with each string as offset to lip_string_t
 * [uint32_t...]: nested function offsets
 * [lip_instruction_t...]: instructions
 * [lip_loc_range_t...]: source locations
 * [lip_string_t...]: string pool, including source name
 * [lip_function_t...]: nested functions
 */
struct lip_function_s
{
	/// Total size, including body
	uint32_t size;

	uint8_t num_args;
	uint8_t is_vararg;
	uint16_t num_locals;
	uint16_t num_imports;
	uint16_t num_constants;
	uint16_t num_instructions;
	uint16_t num_functions;
};

struct lip_function_layout_s
{
	lip_string_t* source_name;
	lip_import_t* imports;
	lip_value_t* constants;
	uint32_t* function_offsets;
	lip_instruction_t* instructions;
	lip_loc_range_t* locations;
};

struct lip_closure_s
{
	union
	{
		lip_function_t* lip;
		lip_native_fn_t native;
	} function;

	lip_string_t* debug_name;

	unsigned env_len:8;
	unsigned is_native:1;

	LIP_FLEXIBLE_ARRAY_MEMBER(lip_value_t, environment);
};

struct lip_stack_frame_s
{
	lip_closure_t* closure;
	lip_instruction_t* pc;
	lip_value_t* ep;
	lip_value_t* bp;

	const char* native_function;
	const char* native_filename;
	int native_line;

	uint8_t num_args;
};

struct lip_vm_s
{
	lip_vm_config_t config;
	lip_runtime_interface_t* rt;

	void* mem;
	lip_value_t* sp;
	lip_stack_frame_t* fp;
	lip_vm_hook_t* hook;
};

struct lip_string_t_alignment_helper
{
	size_t size; char ptr[1];
};

static const size_t lip_string_t_alignment =
	LIP_ALIGN_OF(struct lip_string_t_alignment_helper);

struct lip_function_t_alignment_helper
{
	lip_function_t function;
	lip_import_t import;
	uint32_t index;
	lip_instruction_t instruction;
	lip_loc_range_t location;
};

// I don't know a better way too do this at compile-time
static const size_t lip_function_t_alignment =
	LIP_MAX(
		LIP_ALIGN_OF(struct lip_string_t_alignment_helper),
		LIP_ALIGN_OF(struct lip_function_t_alignment_helper)
	);

LIP_API size_t
lip_vm_memory_required(const lip_vm_config_t* config);

LIP_API void
lip_vm_init(
	lip_vm_t* vm,
	const lip_vm_config_t* config,
	lip_runtime_interface_t* rt,
	void* mem
);

LIP_MAYBE_UNUSED static inline void
lip_function_layout(const lip_function_t* function, lip_function_layout_t* layout)
{
	char* function_end = (char*)function + sizeof(lip_function_t);
	layout->source_name = (lip_string_t*)lip_align_ptr(function_end, lip_string_t_alignment);

	char* source_name_end =
		(char*)layout->source_name
		+ sizeof(lip_string_t)
		+ layout->source_name->length;
	layout->imports = (lip_import_t*)lip_align_ptr(source_name_end, LIP_ALIGN_OF(lip_import_t));
	layout->constants = (lip_value_t*)lip_align_ptr(
		layout->imports + function->num_imports, LIP_ALIGN_OF(lip_value_t)
	);
	layout->function_offsets = (uint32_t*)lip_align_ptr(
		layout->constants + function->num_constants, LIP_ALIGN_OF(uint32_t)
	);
	layout->instructions = (lip_instruction_t*)lip_align_ptr(
		layout->function_offsets + function->num_functions, LIP_ALIGN_OF(lip_instruction_t)
	);
	layout->locations = (lip_loc_range_t*)lip_align_ptr(
		layout->instructions + function->num_instructions, LIP_ALIGN_OF(lip_loc_range_t)
	);
}

LIP_MAYBE_UNUSED static inline void*
lip_function_resource(const lip_function_t* function, uint32_t offset)
{
	return ((char*)function + offset);
}

LIP_MAYBE_UNUSED static inline bool
lip_stack_frame_is_native(lip_stack_frame_t* frame)
{
	return frame->closure == NULL || frame->closure->is_native;
}

LIP_MAYBE_UNUSED static inline lip_memblock_info_t
lip_vm_memory_layout(
	const lip_vm_config_t* config,
	lip_memblock_info_t* os_block,
	lip_memblock_info_t* env_block,
	lip_memblock_info_t* cs_block
)
{
	os_block->element_size = sizeof(lip_value_t);
	os_block->num_elements = config->os_len;
	os_block->alignment = LIP_ALIGN_OF(lip_value_t);

	env_block->element_size = sizeof(lip_value_t);
	env_block->num_elements = config->env_len;
	env_block->alignment = LIP_ALIGN_OF(lip_value_t);

	cs_block->element_size = sizeof(lip_stack_frame_t);
	cs_block->num_elements = config->cs_len;
	cs_block->alignment = LIP_ALIGN_OF(lip_stack_frame_t);

	lip_memblock_info_t* mem_layout[] = { os_block, env_block, cs_block };
	return lip_align_memblocks(LIP_STATIC_ARRAY_LEN(mem_layout), mem_layout);
}

#ifdef _MSC_VER
#	pragma warning(pop)
#endif

#endif
