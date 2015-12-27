#include <stdlib.h>
#include <lip/linker.h>
#include "module.h"
#include "asm.h"

lip_exec_status_t add(lip_vm_t* vm)
{
	lip_value_t* lhs = lip_vm_get_arg(vm, 0);
	lip_value_t* rhs = lip_vm_get_arg(vm, 1);
	// TODO: error handling
	lip_vm_push_number(vm, lhs->data.number + rhs->data.number);
	return LIP_EXEC_OK;
}

lip_exec_status_t lt(lip_vm_t* vm)
{
	lip_value_t* lhs = lip_vm_get_arg(vm, 0);
	lip_value_t* rhs = lip_vm_get_arg(vm, 1);
	// TODO: error handling
	lip_vm_push_boolean(vm, lhs->data.number < rhs->data.number);
	return LIP_EXEC_OK;
}

static void hook(lip_vm_t* vm, void* context)
{
	printf("Call stack depth: %ld\n", vm->fp - vm->min_fp);
	for(lip_value_t* itr = vm->min_sp; itr < vm->sp; ++itr)
	{
		lip_value_print(itr, 1);
		printf("\n");
	}
	printf("%ld: ", vm->ctx.pc - vm->ctx.closure->function_ptr.lip->instructions);
	lip_asm_print(*vm->ctx.pc);
	printf("\n");
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

	const char* fib_sym = "fib";
	lip_string_ref_t fib_ref = { strlen(fib_sym), fib_sym };
	const char* lt_sym = "<";
	lip_string_ref_t lt_ref = { strlen(lt_sym), lt_sym };

	lip_asm_begin(&lasm);
	lip_asm_index_t param = lip_asm_new_local(&lasm);
	lip_asm_index_t fib_import = lip_asm_new_import(&lasm, fib_ref);
	lip_asm_index_t plus_import = lip_asm_new_import(&lasm, plus_ref);
	lip_asm_index_t lt_import = lip_asm_new_import(&lasm, lt_ref);
	lip_asm_index_t else_label = lip_asm_new_label(&lasm);
	lip_asm_index_t done_label = lip_asm_new_label(&lasm);
	lip_asm_add(&lasm,
		LIP_OP_LDL, param,
		LIP_OP_LDI, 3,
		LIP_OP_LDS, lt_import,
		LIP_OP_CALL, 2,
		LIP_OP_JOF, else_label,
		LIP_OP_LDI, -1,
		LIP_OP_LDL, param,
		LIP_OP_LDS, plus_import,
		LIP_OP_CALL, 2,
		LIP_OP_LDS, fib_import,
		LIP_OP_CALL, 1,
		LIP_OP_LDI, -2,
		LIP_OP_LDL, param,
		LIP_OP_LDS, plus_import,
		LIP_OP_CALL, 2,
		LIP_OP_LDS, fib_import,
		LIP_OP_CALL, 1,
		LIP_OP_LDS, plus_import,
		LIP_OP_CALL, 2,
		LIP_OP_JMP, done_label,
		LIP_OP_LABEL, else_label,
		LIP_OP_LDI, 1,
		LIP_OP_LABEL, done_label,
		LIP_OP_RET, 0,
		LIP_ASM_END
	);
	lip_function_t* fib_fn = lip_asm_end(&lasm);

	fib_fn->arity = 1;
	lip_bundler_add_lip_function(&bundler, fib_ref, fib_fn);
	lip_bundler_add_native_function(&bundler, lt_ref, lt, 2);

	lip_module_t* module2 = lip_bundler_end(&bundler);

	lip_linker_t linker;
	lip_linker_init(&linker, lip_default_allocator);
	lip_linker_add_module(&linker, module);
	lip_linker_add_module(&linker, module2);
	lip_linker_link_modules(&linker);
	lip_module_print(module);
	lip_module_print(module2);

	printf("\n");
	lip_vm_t vm;
	size_t mem_required = lip_vm_config(&vm, &linker, 256, 256, 256);
	void* mem = lip_malloc(lip_default_allocator, mem_required);
	const char* sym = "fib";
	lip_string_ref_t sym_ref = { strlen(sym), sym };
	lip_vm_init(&vm, mem);
	(void)hook;
	lip_vm_push_number(&vm, 40);
	lip_value_t value;
	lip_linker_find_symbol(&linker, sym_ref, &value);
	lip_vm_push_value(&vm, &value);
	lip_vm_call(&vm, 1);
	lip_value_print(lip_vm_pop(&vm), 0);
	printf("\n");

	lip_free(lip_default_allocator, mem);

	lip_linker_cleanup(&linker);

	lip_free(lip_default_allocator, fib_fn);
	lip_free(lip_default_allocator, module2);
	lip_free(lip_default_allocator, module);
	lip_bundler_cleanup(&bundler);

	lip_free(lip_default_allocator, function);
	lip_asm_cleanup(&lasm);

	return EXIT_SUCCESS;
}
