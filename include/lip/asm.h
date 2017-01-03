#ifndef LIP_ASM_H
#define LIP_ASM_H

#include <lip/extra.h>
#include "opcode.h"
#include "memory.h"

#define LIP_OP_LABEL 0xFF

typedef uint32_t lip_asm_index_t;
typedef struct lip_asm_s lip_asm_t;
typedef struct lip_tagged_instruction_s lip_tagged_instruction_t;

struct lip_tagged_instruction_s
{
	lip_instruction_t instruction;
	lip_loc_range_t location;
};

struct lip_asm_s
{
	lip_allocator_t* allocator;
	lip_string_ref_t source_name;
	lip_loc_range_t location;
	lip_array(lip_asm_index_t) labels;
	lip_array(lip_asm_index_t) jumps;
	lip_array(lip_tagged_instruction_t) instructions;
	lip_array(lip_function_t*) functions;
	lip_array(uint32_t) imports;
	lip_array(lip_value_t) constants;
	lip_array(lip_string_ref_t) string_pool;
	lip_array(lip_memblock_info_t) string_layout;
	lip_array(lip_memblock_info_t) nested_layout;
	lip_array(lip_memblock_info_t*) function_layout;
};

LIP_API void
lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator);

LIP_API void
lip_asm_cleanup(lip_asm_t* lasm);

LIP_API void
lip_asm_begin(lip_asm_t* lasm, lip_string_ref_t source_name, lip_loc_range_t location);

LIP_API void
lip_asm_add(
	lip_asm_t* lasm,
	lip_opcode_t opcode,
	lip_operand_t operand,
	lip_loc_range_t location
);

LIP_API lip_asm_index_t
lip_asm_new_label(lip_asm_t* lasm);

LIP_API lip_asm_index_t
lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function);

LIP_API lip_asm_index_t
lip_asm_alloc_import(lip_asm_t* lasm, lip_string_ref_t import);

LIP_API lip_asm_index_t
lip_asm_alloc_numeric_constant(lip_asm_t* lasm, double number);

LIP_API lip_asm_index_t
lip_asm_alloc_string_constant(lip_asm_t* lasm, lip_string_ref_t string);

LIP_API lip_asm_index_t
lip_asm_alloc_symbol(lip_asm_t* lasm, lip_string_ref_t string);

LIP_API lip_function_t*
lip_asm_end(lip_asm_t* lasm, lip_allocator_t* allocator);

LIP_MAYBE_UNUSED static inline lip_instruction_t
lip_asm(lip_opcode_t opcode, lip_operand_t operand)
{
	return (((uint32_t)opcode & 0xFF) << 24) | (operand & 0x00FFFFFF);
}

LIP_MAYBE_UNUSED static inline void
lip_disasm(lip_instruction_t instr, lip_opcode_t* opcode, lip_operand_t* operand)
{
	*opcode = (lip_opcode_t)((instr >> 24) & 0xFF);
	*operand = (lip_operand_t)((int32_t)((uint32_t)instr << 8) >> 8);
}

#endif
