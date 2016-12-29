#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/print.h>
#include <lip/vm.h>
#include "munit.h"
#include "test_helpers.h"

#define SOURCE_NAME __FILE__ ":"  STRINGIFY(__LINE__)
#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X

#define lip_assert_num_result(code, result_value) \
	do { \
		test_hook_t hook = { .printed = false, .vtable = { .step = step } }; \
		lip_set_vm_hook(vm, &hook.vtable); \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		munit_assert_not_null(script); \
		lip_print_closure(100, 0, lip_null, (lip_closure_t*)script); \
		lip_value_t result; \
		lip_assert_enum(lip_exec_status_t, LIP_EXEC_OK, ==, lip_exec_script(vm, script, &result)); \
		lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type); \
		munit_assert_double_equal(result_value, result.data.number, 3); \
	} while(0)

#define lip_assert_str_result(code, result_value) \
	do { \
		test_hook_t hook = { .printed = false, .vtable = { .step = step } }; \
		lip_set_vm_hook(vm, &hook.vtable); \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		munit_assert_not_null(script); \
		lip_print_closure(100, 0, lip_null, (lip_closure_t*)script); \
		lip_value_t result; \
		lip_assert_enum(lip_exec_status_t, LIP_EXEC_OK, ==, lip_exec_script(vm, script, &result)); \
		lip_assert_enum(lip_value_type_t, LIP_VAL_STRING, ==, result.type); \
		lip_string_t* returned_str = result.data.reference; \
		lip_assert_mem_equal(result_value, (sizeof(result_value) - 1), returned_str->ptr, returned_str->length); \
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

typedef struct lip_fixture_s lip_fixture_t;
typedef struct test_hook_s test_hook_t;

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

static void
step(lip_vm_hook_t* vtable, const lip_vm_t* vm)
{
	test_hook_t* hook = LIP_CONTAINER_OF(vtable, test_hook_t, vtable);
	if(!hook->printed)
	{
		hook->printed = true;
		lip_print_closure(10, 0, lip_stderr(), vm->fp->closure);
	}
}

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	lip_fixture_t* fixture = lip_new(lip_default_allocator, lip_fixture_t);
	fixture->runtime = lip_create_runtime(lip_default_allocator);
	fixture->context = lip_create_context(fixture->runtime, NULL);
	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	fixture->vm = lip_create_vm(fixture->context, &vm_config);
	return fixture;
}

static void
teardown(void* fixture_)
{
	lip_fixture_t* fixture = fixture_;
	lip_destroy_runtime(fixture->runtime);
	lip_free(lip_default_allocator, fixture);
}

static MunitResult
basic_forms(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;

	lip_assert_num_result("2", 2.0);
	lip_assert_num_result("((fn (x y) (x y)) (fn (x) x) 3.5)", 3.5);
	lip_assert_num_result(
		"(let ((x 1)"
		"      (y 2.5))"
		"    y)",
		2.5
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((x 2.3)) x))",
		2.3
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((x 2.3)) y))",
		2.5
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((test-capture (fn () x)))"
		"      (let ((x 4)) (test-capture))))",
		1.6
	);
	lip_assert_num_result(
		"(letrec ((test-capture (let ((y x)) (fn () y)))"
		"         (x 2.5))"
		"    (test-capture))",
		2.5
	);
	lip_assert_num_result(
		"(letrec ((y x)"
		"         (x 2.5))"
		"    y)",
		2.5
	);
	lip_assert_num_result("(if true 2 3)", 2.0);
	lip_assert_num_result("(if false 2 3)", 3.0);
	lip_assert_num_result("(if nil 2 3)", 3.0);
	lip_assert_num_result("(if 0 2 3)", 2.0);
	lip_assert_num_result(
		"(do"
		" (let ((x 2)) x)"
		" (let ((y 3) (x 1)) x))",
		1.0
	);
	lip_assert_num_result(
		"(let ((x 2))"
		" (let ((y 3)"
		"       (x 1))"
		"  nil)"
		" (let ((z 8)) x))",
		2.0
	);
	lip_assert_num_result(
		"(let ((x 2))"
		" (let ((y 3)"
		"       (x 1))"
		"  nil)"
		" (let ((z 6) (h x) (w 4)) h))",
		2.0
	);
	lip_assert_num_result("(if (if true 2) 1 2)", 1.0);
	lip_assert_num_result("(if (if false 2) 1 2)", 2.0);
	lip_assert_num_result("(((letrec ((x 1.5)) (fn () (fn () x)))))", 1.5);

	return MUNIT_OK;
}

static MunitResult
builtins(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_num_result("(identity 5)", 5.0);

	return MUNIT_OK;
}

static MunitResult
syntax_error(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;

	lip_assert_syntax_error(" 1a  ", "Malformed number", 1, 2, 1, 3);
	lip_assert_syntax_error("  (56  ", "Unterminated list", 1, 3, 1, 3);
	lip_assert_syntax_error(
		"  (let ((x\n"
		"         (fn (x)\n"
		"          (if x true))))\n"
		"   () (x x))  ",
		"Empty list is invalid",
		4, 4, 4, 5
	);
	lip_assert_syntax_error(
		"  (let (x\n"
		"         (fn (x)\n"
		"          (if x true)))\n"
		"   () (x x))  ",
		"a binding must have the form: (<symbol> <expr>)",
		1, 9, 1, 9
	);
	lip_assert_syntax_error(
		"  (let x)  ",
		"'let' must have the form: (let (<bindings...>) <exp...>)",
		1, 3, 1, 9
	);

	return MUNIT_OK;
}

static MunitResult
strings(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_str_result("\"abc\"", "abc");
	lip_assert_str_result("\"ab c \\n \" ", "ab c \n ");
	lip_assert_str_result("\"ab c \\\" \" ", "ab c \" ");
	lip_assert_str_result(
		"(identity \"start \\r \\n\\t\\\\\\a \\b\\f\\v stop\")",
		"start \r \n\t\\\a \b\f\v stop"
	);

	lip_assert_str_result("\"\\x23yay\"", "\x23yay");
	lip_assert_str_result("\"\\x6Fzz\"", "\x6fzz");
	lip_assert_str_result("\"\\xe\"", "\xe");
	lip_assert_str_result("\"\\xfg z\"", "\xfg z");

	lip_assert_str_result("\"\\060yay\"", "\60yay");
	lip_assert_str_result("\"\\178z\"", "\178z");
	lip_assert_str_result("\"\\17 z\"", "\17 z");
	lip_assert_str_result("\"\\15\"", "\15");

	lip_assert_syntax_error(
		"\" \\xhh\"",
		"Invalid hex escape sequence",
		1, 3, 1, 4
	);
	lip_assert_syntax_error(
		"\"  \\777 \"",
		"Invalid octal escape sequence",
		1, 4, 1, 7
	);
	lip_assert_syntax_error(
		"\" \\9 \"",
		"Invalid octal escape sequence",
		1, 3, 1, 4
	);
	lip_assert_syntax_error(
		"\" \\9\"",
		"Invalid octal escape sequence",
		1, 3, 1, 4
	);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/basic_forms",
		.test = basic_forms,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/syntax_error",
		.test = syntax_error,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/builtins",
		.test = builtins,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/strings",
		.test = strings,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite runtime = {
	.prefix = "/runtime",
	.tests = tests
};
