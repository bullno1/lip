#include <stdlib.h>
#include <lip/allocator.h>
#include <lip/module.h>
#include <lip/bundler.h>
#include "asm.h"

lip_exec_status_t print(lip_vm_t* vm)
{
	return LIP_EXEC_OK;
}

int main(int argc, char* argv[])
{
	lip_asm_t lasm;
	lip_asm_init(&lasm, lip_default_allocator);

	lip_function_t* function = load_asm(&lasm);
	function->arity = 0;

	lip_bundler_t bundler;
	lip_bundler_init(&bundler, lip_default_allocator);

	lip_bundler_begin(&bundler);

	const char* main_sym = "main";
	lip_string_ref_t main_ref = { strlen(main_sym), main_sym };
	lip_bundler_add_lip_function(&bundler, main_ref, function);

	const char* print_sym = "print";
	lip_string_ref_t print_ref = { strlen(print_sym), print_sym };
	lip_bundler_add_native_function(&bundler, print_ref, 0, print);

	lip_module_t* module = lip_bundler_end(&bundler);
	lip_module_print(module);

	lip_free(lip_default_allocator, module);
	lip_bundler_cleanup(&bundler);

	lip_free(lip_default_allocator, function);
	lip_asm_cleanup(&lasm);

	return EXIT_SUCCESS;
}
