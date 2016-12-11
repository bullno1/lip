#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/arena_allocator.h>
#include <lip/io.h>
#include "munit.h"
#include "test_helpers.h"

typedef struct lip_fixture_s lip_fixture_t;

struct lip_fixture_s
{
	lip_allocator_t* arena_allocator;
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
	lip_allocator_t* arena_allocator = lip_arena_allocator_create(lip_default_allocator, 1024);
	fixture->arena_allocator = arena_allocator;
	fixture->runtime = lip_create_runtime(lip_default_allocator);
	fixture->context = lip_create_context(fixture->runtime, NULL);
	lip_vm_config_t vm_config = {
		.allocator = arena_allocator,
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
	lip_arena_allocator_destroy(fixture->arena_allocator);
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

static MunitTest tests[] = {
	{
		.name = "/basic_forms",
		.test = basic_forms,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite runtime = {
	.prefix = "/runtime",
	.tests = tests
};
