#ifndef LIP_ASM_H
#define LIP_ASM_H

#include "common.h"
#include "opcode.h"

#define LIP_OP_LABEL 0xFF

typedef uint32_t lip_asm_index_t;
typedef struct lip_asm_s lip_asm_t;

lip_asm_t*
lip_asm_create(lip_allocator_t* allocator);

void
lip_asm_destroy(lip_asm_t* lasm);

void
lip_asm_begin(lip_asm_t* lasm, lip_string_ref_t source_name);

void
lip_asm_add(
	lip_asm_t* lasm,
	lip_opcode_t opcode,
	lip_operand_t operand,
	lip_loc_range_t location
);

lip_asm_index_t
lip_asm_new_label(lip_asm_t* lasm);

lip_asm_index_t
lip_asm_new_local(lip_asm_t* lasm);

lip_asm_index_t
lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function);

lip_asm_index_t
lip_asm_alloc_import(lip_asm_t* lasm, lip_string_ref_t import);

lip_asm_index_t
lip_asm_alloc_numeric_constant(lip_asm_t* lasm, double number);

lip_asm_index_t
lip_asm_alloc_string_constant(lip_asm_t* lasm, lip_string_ref_t string);

lip_function_t*
lip_asm_end(lip_asm_t* lasm);

LIP_MAYBE_UNUSED static inline lip_instruction_t
lip_asm(lip_opcode_t opcode, lip_operand_t operand)
{
	return (((int32_t)opcode & 0xFF) << 24) | (operand & 0x00FFFFFF);
}

LIP_MAYBE_UNUSED static inline void
lip_disasm(lip_instruction_t instr, lip_opcode_t* opcode, lip_operand_t* operand)
{
	*opcode = (instr >> 24) & 0xFF;
	*operand = (int32_t)((uint32_t)instr << 8) >> 8;
}

#endif
