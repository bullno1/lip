#ifndef LIP_TEST_RUNTIME_HELPERS_H
#define LIP_TEST_RUNTIME_HELPERS_H

#include <lip/lip.h>
#include <lip/io.h>
#include <lip/memory.h>
#include <lip/print.h>
#include <lip/vm.h>
#include "test_helpers.h"

#define SOURCE_NAME __FILE__ ":"  STRINGIFY(__LINE__)
#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X

#define lip_assert_result(code, expected_status, assert_fn, ...) \
	do { \
		test_hook_t hook = { .printed = false, .vtable = { .step = step } }; \
		lip_set_vm_hook(vm, &hook.vtable); \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		if(script == NULL) { lip_print_error(lip_stderr(), ctx); } \
		munit_assert_not_null(script); \
		lip_print_script(100, 0, lip_null, script); \
		lip_value_t result; \
		lip_reset_vm(vm); \
		lip_exec_status_t status = lip_exec_script(vm, script, &result); \
		if(status != LIP_EXEC_OK) { lip_print_error(lip_stderr(), ctx); } \
		lip_assert_enum(lip_exec_status_t, expected_status, ==, status); \
		assert_fn(lip_pp_nth(1, (__VA_ARGS__), 0), result); \
		lip_unload_script(ctx, script); \
	} while(0)

#define lip_assert_num_result(code, ...) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_num, __VA_ARGS__)

#define lip_assert_str_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_str, result_value)

#define lip_assert_symbol_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_symbol, result_value)

#define lip_assert_nil_result(code) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_nil, placeholder)

#define lip_assert_boolean_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_boolean, result_value)

#define lip_assert_pass(expected, actual)

#define lip_assert_error_msg(code, msg) \
	do { \
		lip_assert_result(code, LIP_EXEC_ERROR, lip_assert_pass, msg); \
		const lip_context_error_t* error = lip_get_error(ctx); \
		lip_assert_string_ref_equal(lip_string_ref(msg), error->message); \
	} while(0)

#define lip_assert_syntax_error(code, error_msg, start_line, start_col, end_line, end_col) \
	do { \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		munit_assert_null(script); \
		const lip_context_error_t* error = lip_get_error(ctx); \
		lip_assert_string_ref_equal(lip_string_ref("Syntax error"), error->message); \
		lip_assert_string_ref_equal(lip_string_ref(error_msg), error->records[0].message); \
		munit_assert_uint(1, ==, error->num_records); \
		lip_loc_range_t expected_location = { \
			.start = { .line = start_line, .column = start_col }, \
			.end = { .line = end_line, .column = end_col } \
		}; \
		lip_assert_loc_range_equal(expected_location, error->records[0].location); \
	} while(0)

#define LIP_STRING_REF_LITERAL(str) \
	{ .length = LIP_STATIC_ARRAY_LEN(str), .ptr = str}

typedef struct lip_fixture_s lip_fixture_t;
typedef struct test_hook_s test_hook_t;

static const lip_string_ref_t module_search_patterns[] = {
	LIP_STRING_REF_LITERAL("src/tests/?.lip"),
	LIP_STRING_REF_LITERAL("src/tests/!.lip")
};

struct test_hook_s
{
	lip_vm_hook_t vtable;
	bool printed;
};

struct lip_fixture_s
{
	lip_runtime_t* runtime;
	lip_context_t* context;
	lip_vm_t* vm;
};

LIP_MAYBE_UNUSED static void
step(lip_vm_hook_t* vtable, const lip_vm_t* vm)
{
	test_hook_t* hook = LIP_CONTAINER_OF(vtable, test_hook_t, vtable);
	if(!hook->printed)
	{
		hook->printed = true;
		lip_print_closure(10, 0, lip_stderr(), vm->fp->closure);
	}
}

LIP_MAYBE_UNUSED static void
teardown(void* fixture_)
{
	lip_fixture_t* fixture = fixture_;
	lip_destroy_vm(fixture->context, fixture->vm);
	lip_destroy_context(fixture->context);
	lip_destroy_runtime(fixture->runtime);
	lip_free(lip_default_allocator, fixture);
}

LIP_MAYBE_UNUSED static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	lip_runtime_config_t cfg;
	lip_reset_runtime_config(&cfg);
	cfg.module_search_patterns = module_search_patterns;
	cfg.num_module_search_patterns = LIP_STATIC_ARRAY_LEN(module_search_patterns);
	lip_fixture_t* fixture = lip_new(lip_default_allocator, lip_fixture_t);
	fixture->runtime = lip_create_runtime(&cfg);
	fixture->context = lip_create_context(fixture->runtime, NULL);
	fixture->vm = lip_create_vm(fixture->context, NULL);
	return fixture;
}

#endif
