#include <lip/ex/vm.h>
#include <lip/asm.h>
#include <lip/memory.h>
#include <lip/temp_allocator.h>
#include "munit.h"
#include "test_helpers.h"

static MunitResult
fibonacci(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_asm_t* lasm = lip_asm_create(lip_default_allocator);
	lip_asm_begin(lasm, lip_string_ref("fib.lip"));
	lip_asm_index_t else_label = lip_asm_new_label(lasm);
	lip_asm_index_t done_label = lip_asm_new_label(lasm);
	lip_asm_index_t n = lip_asm_new_local(lasm);

	// n < 2
	lip_asm_add(lasm, LIP_OP_LDI, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LT, 0, LIP_LOC_NOWHERE);

	// then
	lip_asm_add(lasm, LIP_OP_JOF, else_label, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDI, 1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_JMP, done_label, LIP_LOC_NOWHERE);

	// else
	lip_asm_add(lasm, LIP_OP_LABEL, else_label, LIP_LOC_NOWHERE);
	// fib(n + (-1))
	lip_asm_add(lasm, LIP_OP_LDI, -1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, 0, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);

	// fib(n + (-2))
	lip_asm_add(lasm, LIP_OP_LDI, -2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, 0, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_LABEL, done_label, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RET, 0, LIP_LOC_NOWHERE);

	lip_function_t* fib_fn = lip_asm_end(lasm);

	lip_asm_begin(lasm, lip_string_ref("fib.lip"));
	lip_asm_index_t n1 = lip_asm_new_local(lasm);
	lip_asm_index_t fib = lip_asm_new_local(lasm);
	lip_asm_index_t fib_fn_index = lip_asm_new_function(lasm, fib_fn);

	lip_asm_add(lasm, LIP_OP_PLHR, fib, LIP_LOC_NOWHERE);

	uint16_t num_captures = 1;
	lip_asm_add(
		lasm,
		LIP_OP_CLS, (fib_fn_index & 0xFFF) | ((num_captures & 0xFFF) << 12),
		LIP_LOC_NOWHERE
	);
	lip_asm_add(lasm, LIP_OP_LDL, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_SET, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RCLS, fib, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_LDL, n1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDL, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RET, 0, LIP_LOC_NOWHERE);

	lip_function_t* fn = lip_asm_end(lasm);
	lip_free(lip_default_allocator, fib_fn);
	lip_asm_destroy(lasm);

	lip_closure_t* closure = lip_new(lip_default_allocator, lip_closure_t);
	closure->is_native = false;
	closure->env_len = 0;
	closure->function.lip = fn;

	lip_allocator_t* temp_allocator =
		lip_temp_allocator_create(lip_default_allocator, 2048);
	lip_vm_config_t vm_config = {
		.allocator = temp_allocator,
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	lip_vm_t* vm = lip_vm_create(lip_default_allocator, &vm_config);
	lip_vm_push_number(vm, 8);
	lip_vm_push_value(vm, (lip_value_t){
		.type = LIP_VAL_FUNCTION,
		.data = {.reference = closure}
	});

	lip_value_t result;
	munit_assert_int(LIP_EXEC_OK, ==, lip_vm_call(vm, 1, &result));
	munit_assert_int(LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(34.0, result.data.number, 0);

	lip_vm_destroy(lip_default_allocator, vm);
	lip_free(lip_default_allocator, closure);
	lip_free(lip_default_allocator, fn);
	lip_temp_allocator_destroy(temp_allocator);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/fibonacci",
		.test = fibonacci,
		.setup = NULL,
		.tear_down = NULL
	},
	{ .test = NULL }
};

MunitSuite vm = {
	.prefix = "/vm",
	.tests = tests
};
