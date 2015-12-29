#ifndef LIP_ASM_H
#define LIP_ASM_H

#include <stdint.h>
#include "types.h"
#include "function.h"

#define LIP_ASM_END 0xFE
#define LIP_OP_LABEL 0xFF
#define LIP_ASM(ASM, ...) lip_asm_add(ASM, __VA_ARGS__, LIP_ASM_END)

typedef int32_t lip_asm_index_t;

typedef struct lip_asm_t
{
	lip_allocator_t* allocator;
	lip_array(lip_asm_index_t) labels;
	lip_array(lip_asm_index_t) jumps;
	lip_array(lip_instruction_t) instructions;
	lip_array(lip_value_t) constants;
	lip_array(lip_function_t) functions;
	lip_array(lip_string_ref_t) import_symbols;
	lip_array(char) string_pool;
	lip_asm_index_t num_locals;
} lip_asm_t;

lip_instruction_t lip_asm(lip_opcode_t opcode, int32_t operand);
void lip_disasm(
	lip_instruction_t instruction, lip_opcode_t* opcode, int32_t* operand
);
void lip_asm_print(lip_instruction_t instruction);

void lip_asm_init(lip_asm_t* lasm, lip_allocator_t* allocator);
void lip_asm_begin(lip_asm_t* lasm);
void lip_asm_add(lip_asm_t* lasm, ...);
lip_asm_index_t lip_asm_new_label(lip_asm_t* lasm);
lip_asm_index_t lip_asm_new_local(lip_asm_t* lasm);
lip_asm_index_t lip_asm_new_number_const(lip_asm_t* lasm, double number);
lip_asm_index_t lip_asm_new_string_const(lip_asm_t* lasm, lip_string_ref_t value);
lip_asm_index_t lip_asm_new_function(lip_asm_t* lasm, lip_function_t* function);
lip_asm_index_t lip_asm_new_import(lip_asm_t* lasm, lip_string_ref_t symbol);
lip_function_t* lip_asm_end(lip_asm_t* lasm);
void lip_asm_cleanup(lip_asm_t* lasm);

#endif
