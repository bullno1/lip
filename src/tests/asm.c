#include <stdio.h>
#include <lip/asm.h>
#include <lip/allocator.h>
#include "asm.h"

int main(int argc, char* argv[])
{
	lip_asm_t lasm;
	lip_asm_init(&lasm, lip_default_allocator);

	lip_function_t* function = load_asm(&lasm);
	function->arity = 0;
	lip_function_print(function, 0);
	lip_free(lip_default_allocator, function);

	lip_asm_cleanup(&lasm);

	return EXIT_SUCCESS;
}
