#include <lip/runtime.h>
#include <lip/lexer.h>
#include <lip/parser.h>
#include <lip/memory.h>
#include <lip/temp_allocator.h>
#include <lip/io.h>
#include "munit.h"
#include "test_helpers.h"

typedef struct lip_fixture_s lip_fixture_t;

struct lip_fixture_s
{
	lip_allocator_t* temp_allocator;
	lip_runtime_t* runtime;
};

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	lip_fixture_t* fixture = lip_new(lip_default_allocator, lip_fixture_t);
	lip_allocator_t* temp_allocator = lip_temp_allocator_create(lip_default_allocator, 1024);
	lip_vm_config_t vm_config = {
		.allocator = temp_allocator,
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	fixture->temp_allocator = temp_allocator;
	lip_runtime_config_t runtime_config = {
		.vm_config = vm_config
	};
	lip_runtime_t* runtime = lip_runtime_create(lip_default_allocator, &runtime_config);
	fixture->runtime = runtime;
	return fixture;
}

static void
teardown(void* fixture_)
{
	lip_fixture_t* fixture = fixture_;
	lip_runtime_destroy(fixture->runtime);
	lip_temp_allocator_destroy(fixture->temp_allocator);
	lip_free(lip_default_allocator, fixture);
}

static bool
lip_runtime_exec_string(
	lip_runtime_t* runtime,
	const char* string,
	lip_value_t* result
)
{
	struct lip_sstream_s sstream;

	return lip_runtime_exec(
		runtime,
		lip_make_sstream(lip_string_ref(string), &sstream),
		lip_string_ref(__func__),
		NULL,
		result
	);
}

static MunitResult
basic_forms(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_runtime_t* runtime = fixture->runtime;

	lip_value_t result;

	munit_assert_true(lip_runtime_exec_string(runtime, "2", &result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime, "((fn (x y) (x y)) (fn (x) x) 3.5)", &result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.5, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1)"
			"      (y 2.5))"
			"    y)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.3, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) y))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((test-capture (fn () x)))"
			"      (let ((x 4)) (test-capture))))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.6, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(letrec ((test-capture (let ((y x)) (fn () y)))"
			"         (x 2.5))"
			"    (test-capture))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(letrec ((y x)"
			"         (x 2.5))"
			"    y)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(if true 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(if false 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(if nil 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(if 0 2 3)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(do"
		    " (let ((x 2)) x)"
			" (let ((y 3) (x 1)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
		    "(let ((x 2))"
			" (let ((y 3)"
			"       (x 1))"
			"  nil)"
			" (let ((z 8)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
		    "(let ((x 2))"
			" (let ((y 3)"
			"       (x 1))"
			"  nil)"
			" (let ((z 6) (h x) (w 4)) h))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(if (if true 2) 1 2)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(1.0, result.data.number, 3);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
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
	lip_runtime_t* runtime = fixture->runtime;
	lip_value_t result;
	const lip_error_t* error;

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			" 1a  ",
			&result));
	error = lip_runtime_last_error(runtime);
	const lip_error_t* lex_error = error->extra;
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_LEXER);
	lip_assert_enum(lip_lex_error_t, lex_error->code, ==, LIP_LEX_BAD_NUMBER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 2 },
		.end = { .line = 1, .column = 3 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (56  ",
			&result));
	error = lip_runtime_last_error(runtime);
	const lip_error_t* parse_error = error->extra;
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_PARSER);
	lip_assert_enum(lip_lex_error_t, parse_error->code, ==, LIP_PARSE_UNTERMINATED_LIST);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 3 },
		.end = { .line = 1, .column = 3 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (let ((x\n"
			"         (fn (x)\n"
			"          (if x true))))\n"
			"   () (x x))  ",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 4, .column = 4 },
		.end = { .line = 4, .column = 5 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (let (x\n"
			"         (fn (x)\n"
			"          (if x true)))\n"
			"   () (x x))  ",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 9 },
		.end = { .line = 1, .column = 9 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (let x)  ",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 3 },
		.end = { .line = 1, .column = 9 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"(fn (xy zzz xy) x)",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 13 },
		.end = { .line = 1, .column = 14 }
	}));

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
