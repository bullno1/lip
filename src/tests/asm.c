#include <stdio.h>
#include <string.h>
#include <lip/asm.h>
#include <lip/allocator.h>

int main(int argc, char* argv[])
{
	lip_asm_t lasm;
	lip_asm_init(&lasm, lip_default_allocator);
	lip_asm_begin(&lasm);

	lip_value_t pi = { .type = LIP_VAL_NUMBER, .data = { .number = 3.14 } };
	lip_asm_index_t pi_index = lip_asm_new_constant(&lasm, &pi);

	const char* print = "print";
	lip_asm_index_t print_index = lip_asm_new_import(&lasm, print, strlen(print));

	/*lip_asm_index_t label = lip_asm_new_label(&lasm);*/
	lip_asm_add(&lasm,
		LIP_OP_NOP, 0,
		LIP_OP_LDC, pi_index,
		LIP_OP_LDS, print_index,
		LIP_OP_CALL, 1,
		LIP_ASM_END
	);
	lip_function_t* function = lip_asm_end(&lasm);
	function->arity = 0;
	lip_function_print(function, 0);
	lip_free(lip_default_allocator, function);
	lip_asm_cleanup(&lasm);

	return EXIT_SUCCESS;
}
