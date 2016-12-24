#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/io.h>
#include "munit.h"
#include "test_helpers.h"

typedef struct lip_fixture_s lip_fixture_t;

struct lip_fixture_s
{
	lip_runtime_t* runtime;
	lip_context_t* context;
	lip_vm_t* vm;
};

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

static bool
lip_exec_string(lip_context_t* ctx, lip_vm_t* vm, const char* string, lip_value_t* result)
{
	struct lip_sstream_s sstream;
	lip_in_t* input = lip_make_sstream(lip_string_ref(string), &sstream);
	lip_script_t* script = lip_load_script(ctx, lip_string_ref("test"), input);
	munit_assert_not_null(script);
	return lip_exec_script(vm, script, result) == LIP_EXEC_OK;
}

static MunitResult
basic_forms(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;

	lip_value_t result;

	munit_assert_true(lip_exec_string(ctx, vm, "2", &result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx, vm, "((fn (x y) (x y)) (fn (x) x) 3.5)", &result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.5, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(let ((x 1)"
			"      (y 2.5))"
			"    y)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.3, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) y))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((test-capture (fn () x)))"
			"      (let ((x 4)) (test-capture))))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.6, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(letrec ((test-capture (let ((y x)) (fn () y)))"
			"         (x 2.5))"
			"    (test-capture))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(letrec ((y x)"
			"         (x 2.5))"
			"    y)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if true 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if false 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if nil 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if 0 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(do"
		    " (let ((x 2)) x)"
			" (let ((y 3) (x 1)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
		    "(let ((x 2))"
			" (let ((y 3)"
			"       (x 1))"
			"  nil)"
			" (let ((z 8)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
		    "(let ((x 2))"
			" (let ((y 3)"
			"       (x 1))"
			"  nil)"
			" (let ((z 6) (h x) (w 4)) h))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if (if true 2) 1 2)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.0, result.data.number, 3);

	munit_assert_true(
		lip_exec_string(
			ctx,
			vm,
			"(if (if false 2) 1 2)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	return MUNIT_OK;
}

static MunitResult
syntax_error(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;

#define lip_assert_syntax_error(code, error_msg, start_line, start_col, end_line, end_col) \
	do { \
		struct lip_sstream_s sstream; \
		lip_in_t* input = lip_make_sstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref("test"), input); \
		munit_assert_null(script); \
		const lip_context_error_t* error = lip_get_error(ctx); \
		lip_assert_string_ref_equal(lip_string_ref(error_msg), error->message); \
		munit_assert_uint(1, ==, error->num_records); \
		lip_assert_loc_range_equal(error->records[0].location, ((lip_loc_range_t){ \
			.start = { .line = start_line, .column = start_col }, \
			.end = { .line = end_line, .column = end_col } \
		})); \
		lip_assert_string_ref_equal(lip_string_ref("test"), error->records[0].message); \
	} while(0)

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

#undef lip_assert_syntax_error

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
	{ .test = NULL }
};

MunitSuite runtime = {
	.prefix = "/runtime",
	.tests = tests
};
