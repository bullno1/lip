#include <lip/lip.h>
#include <lip/vm.h>
#include <lip/asm.h>
#include <lip/memory.h>
#include <lip/print.h>
#include <lip/io.h>
#include <lip/arena_allocator.h>
#include "munit.h"
#include "test_helpers.h"

struct dummy_runtime_interface_s
{
	lip_runtime_interface_t vtable;
	lip_allocator_t* allocator;
};

static void*
rt_malloc(lip_runtime_interface_t* vtable, lip_value_type_t type, size_t size)
{
	(void)type;
	struct dummy_runtime_interface_s* self =
		LIP_CONTAINER_OF(vtable, struct dummy_runtime_interface_s, vtable);
	return lip_malloc(self->allocator, size);
}

lip_vm_t*
lip_vm_create(
	lip_allocator_t* allocator, lip_vm_config_t* config, lip_runtime_interface_t* rt
)
{
	lip_memblock_info_t vm_block = {
		.element_size = sizeof(lip_vm_t),
		.num_elements = 1,
		.alignment = LIP_ALIGN_OF(lip_vm_t)
	};

	lip_memblock_info_t vm_mem_block = {
		.element_size = lip_vm_memory_required(config),
		.num_elements = 1,
		.alignment = LIP_MAX_ALIGNMENT
	};

	lip_memblock_info_t* vm_layout[] = { &vm_block, &vm_mem_block };
	lip_memblock_info_t block_info =
		lip_align_memblocks(LIP_STATIC_ARRAY_LEN(vm_layout), vm_layout);

	void* mem = lip_malloc(allocator, block_info.num_elements);
	lip_vm_t* vm = lip_locate_memblock(mem, &vm_block);
	void* vm_mem = lip_locate_memblock(mem, &vm_mem_block);
	lip_vm_init(vm, config, rt, vm_mem);

	return vm;
}

static void
lip_vm_destroy(lip_allocator_t* allocator, lip_vm_t* vm)
{
	lip_free(allocator, vm);
}

static MunitResult
fibonacci(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_asm_t* lasm = lip_asm_create(lip_default_allocator);
	lip_asm_begin(lasm, lip_string_ref("fib.lip"), LIP_LOC_NOWHERE);
	lip_asm_index_t else_label = lip_asm_new_label(lasm);
	lip_asm_index_t done_label = lip_asm_new_label(lasm);
	lip_asm_index_t n = 0;
	lip_asm_index_t fib_index = 0;

	// n < 2
	lip_asm_add(lasm, LIP_OP_LDI, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LARG, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LT, 2, LIP_LOC_NOWHERE);

	// then
	lip_asm_add(lasm, LIP_OP_JOF, else_label, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDI, 1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_JMP, done_label, LIP_LOC_NOWHERE);

	// else
	lip_asm_add(lasm, LIP_OP_LABEL, else_label, LIP_LOC_NOWHERE);
	// fib(n + (-1))
	lip_asm_add(lasm, LIP_OP_LDI, -1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LARG, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDCV, fib_index, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);

	// fib(n + (-2))
	lip_asm_add(lasm, LIP_OP_LDI, -2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LARG, n, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDCV, fib_index, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_ADD, 2, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_LABEL, done_label, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RET, 0, LIP_LOC_NOWHERE);

	lip_function_t* fib_fn = lip_asm_end(lasm, lip_default_allocator);
	fib_fn->num_args = 1;
	fib_fn->num_locals = 1;

	lip_asm_begin(lasm, lip_string_ref("fib.lip"), LIP_LOC_NOWHERE);
	lip_asm_index_t n1 = 0;
	lip_asm_index_t fib = 0;
	lip_asm_index_t fib_fn_index = lip_asm_new_function(lasm, fib_fn);

	lip_asm_add(lasm, LIP_OP_PLHR, fib, LIP_LOC_NOWHERE);

	uint16_t num_captures = 1;
	lip_asm_add(
		lasm,
		LIP_OP_CLS, (fib_fn_index & 0xFFF) | ((num_captures & 0xFFF) << 12),
		LIP_LOC_NOWHERE
	);
	lip_asm_add(lasm, LIP_OP_LDLV, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_SET, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RCLS, fib, LIP_LOC_NOWHERE);

	lip_asm_add(lasm, LIP_OP_LARG, n1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LDLV, fib, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_CALL, 1, LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_RET, 0, LIP_LOC_NOWHERE);

	lip_function_t* fn = lip_asm_end(lasm, lip_default_allocator);
	fn->num_args = 1;
	fn->num_locals = 1;
	lip_free(lip_default_allocator, fib_fn);
	lip_asm_destroy(lasm);

	lip_closure_t* closure = lip_new(lip_default_allocator, lip_closure_t);
	*closure = (lip_closure_t) {
		.is_native = false,
		.env_len = 0,
		.function = { .lip = fn }
	};
	lip_print_closure(10, 0, lip_stderr(), closure);

	lip_allocator_t* arena_allocator =
		lip_arena_allocator_create(lip_default_allocator, 2048);
	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	struct dummy_runtime_interface_s rt = {
		.allocator = arena_allocator,
		.vtable = { .malloc = rt_malloc }
	};
	lip_vm_t* vm = lip_vm_create(lip_default_allocator, &vm_config, &rt.vtable);
	lip_vm_t old_vm_state = *vm;

	lip_value_t result;
	munit_assert_int(
		LIP_EXEC_OK,
		==,
		lip_call(
			vm,
			&result,
			(lip_value_t){ .type = LIP_VAL_FUNCTION, .data = { .reference = closure } },
			1,
			(lip_value_t){ .type = LIP_VAL_NUMBER, .data = { .number = 8 } }
		)
	);
	munit_assert_int(LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(34.0, result.data.number, 0);
	munit_assert_memory_equal(sizeof(lip_vm_t), &old_vm_state, vm);

	lip_vm_destroy(lip_default_allocator, vm);
	lip_free(lip_default_allocator, closure);
	lip_free(lip_default_allocator, fn);
	lip_arena_allocator_destroy(arena_allocator);

	return MUNIT_OK;
}

static lip_exec_status_t
identity(lip_vm_t* vm, lip_value_t* result)
{
	uint8_t num_args;
	const lip_value_t* args = lip_get_args(vm, &num_args);
	*result = args[0];
	return LIP_EXEC_OK;
}

static MunitResult
call_native(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_allocator_t* arena_allocator =
		lip_arena_allocator_create(lip_default_allocator, 2048);

	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	struct dummy_runtime_interface_s rt = {
		.allocator = arena_allocator,
		.vtable = { .malloc = rt_malloc }
	};
	lip_vm_t* vm = lip_vm_create(arena_allocator, &vm_config, &rt.vtable);
	lip_closure_t* closure = lip_new(arena_allocator, lip_closure_t);
	*closure = (lip_closure_t) {
		.is_native = true,
		.env_len = 0,
		.function = { .native = identity }
	};

	lip_value_t result;
	lip_call(
		vm,
		&result,
		(lip_value_t){
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = closure }
		},
		1,
		(lip_value_t){
			.type = LIP_VAL_NUMBER,
			.data = { .number = 42 }
		}
	);
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(42, result.data.number, 2);

	lip_arena_allocator_destroy(arena_allocator);

	return MUNIT_OK;
}

static MunitResult
error(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_asm_t* lasm = lip_asm_create(lip_default_allocator);
	lip_asm_begin(lasm, lip_string_ref(__func__), LIP_LOC_NOWHERE);
	lip_asm_add(lasm, LIP_OP_LABEL - 1, 0, LIP_LOC_NOWHERE);
	lip_function_t* fn = lip_asm_end(lasm, lip_default_allocator);

	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	lip_allocator_t* arena_allocator = lip_arena_allocator_create(lip_default_allocator, 2048);
	struct dummy_runtime_interface_s rt = {
		.allocator = arena_allocator,
		.vtable = { .malloc = rt_malloc }
	};
	lip_vm_t* vm = lip_vm_create(lip_default_allocator, &vm_config, &rt.vtable);
	lip_closure_t* closure = lip_new(lip_default_allocator, lip_closure_t);
	*closure = (lip_closure_t) {
		.is_native = false,
		.env_len = 0,
		.function = { .lip = fn }
	};

	lip_value_t result;
	lip_exec_status_t status = lip_call(vm,
		&result,
		(lip_value_t){ .type = LIP_VAL_FUNCTION, .data = { .reference = closure } },
		0
	);
	lip_assert_enum(lip_exec_status_t, LIP_EXEC_ERROR, ==, status);
	lip_assert_str("Illegal instruction", result);

	lip_free(lip_default_allocator, closure);
	lip_arena_allocator_destroy(arena_allocator);
	lip_vm_destroy(lip_default_allocator, vm);
	lip_asm_destroy(lasm);
	lip_free(lip_default_allocator, fn);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/fibonacci",
		.test = fibonacci,
		.setup = NULL,
		.tear_down = NULL
	},
	{
		.name = "/error",
		.test = error,
		.setup = NULL,
		.tear_down = NULL
	},
	{
		.name = "/call_native",
		.test = call_native,
		.setup = NULL,
		.tear_down = NULL
	},
	{ .test = NULL }
};

MunitSuite vm = {
	.prefix = "/vm",
	.tests = tests
};
