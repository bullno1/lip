#ifndef LIP_TEST_ASM_H
#define LIP_TEST_ASM_H

#include <stdio.h>
#include <string.h>
#include <lip/asm.h>
#include <lip/allocator.h>
#include <lip/utils.h>

static inline lip_function_t* load_asm(lip_asm_t* lasm)
{
	lip_asm_begin(lasm);

	lip_asm_index_t pi_index = lip_asm_new_number_const(lasm, 3.14);
	lip_asm_index_t life_index = lip_asm_new_number_const(lasm, 42.0);

	lip_string_ref_t print_ref = lip_string_ref("print");
	lip_asm_index_t print_index = lip_asm_new_import(lasm, print_ref);

	lip_string_ref_t plus_ref = lip_string_ref("+");
	lip_asm_index_t plus_index = lip_asm_new_import(lasm, plus_ref);

	lip_asm_index_t label = lip_asm_new_label(lasm);
	lip_asm_index_t label2 = lip_asm_new_label(lasm);
	lip_asm_add(lasm,
		LIP_OP_NOP, 0,
		LIP_OP_LABEL, label,
		LIP_OP_LDC, pi_index,
		LIP_OP_LDC, life_index,
		LIP_OP_LDS, plus_index,
		LIP_OP_CALL, 2,
		LIP_OP_LDS, print_index,
		LIP_OP_CALL, 1,
		LIP_OP_RET, 0,
		LIP_OP_POP, 1,
		LIP_OP_JMP, label,
		LIP_OP_LABEL, label2,
		LIP_OP_NOP, 0,
		LIP_OP_JMP, label2,
		LIP_ASM_END
	);

	return lip_asm_end(lasm);
}

#endif
