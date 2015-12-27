#include <stdlib.h>
#include <lip/linker.h>
#include "module.h"

lip_exec_status_t add(lip_vm_t* vm)
{
	lip_value_t* lhs = lip_vm_get_arg(vm, 0);
	lip_value_t* rhs = lip_vm_get_arg(vm, 1);
	// TODO: error handling
	lip_vm_push_number(vm, lhs->data.number + rhs->data.number);
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

	lip_module_t* module = bundle_functions(&bundler, function);

	lip_bundler_begin(&bundler);
	const char* plus_sym = "+";
	lip_string_ref_t plus_ref = { strlen(plus_sym), plus_sym };
	lip_bundler_add_native_function(&bundler, plus_ref, add, 2);
	lip_module_t* module2 = lip_bundler_end(&bundler);

	lip_linker_t linker;
	lip_linker_init(&linker, lip_default_allocator);
	lip_linker_add_module(&linker, module);
	lip_linker_add_module(&linker, module2);
	lip_linker_link_modules(&linker);
	lip_module_print(module);

	printf("\n");
	lip_vm_t vm;
	size_t mem_required = lip_vm_config(&vm, &linker, 16, 16, 16);
	void* mem = lip_malloc(lip_default_allocator, mem_required);
	const char* sym = "main";
	lip_string_ref_t sym_ref = { strlen(sym), sym };
	lip_vm_init(&vm, mem);
	lip_value_t value;
	lip_linker_find_symbol(&linker, sym_ref, &value);
	lip_vm_push_value(&vm, &value);
	lip_vm_call(&vm, 0);
	lip_value_print(lip_vm_pop(&vm), 0);
	printf("\n");

	lip_free(lip_default_allocator, mem);

	lip_linker_cleanup(&linker);

	lip_free(lip_default_allocator, module2);
	lip_free(lip_default_allocator, module);
	lip_bundler_cleanup(&bundler);

	lip_free(lip_default_allocator, function);
	lip_asm_cleanup(&lasm);

	return EXIT_SUCCESS;
}
