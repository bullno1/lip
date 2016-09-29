#include <lip/runtime.h>
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
	munit_assert_double_equal(2.0, result.data.number, 0);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime, "((fn (x y) (x y)) (fn (x) x) 3.5)", &result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(3.0, result.data.number, 0);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1)"
			"      (y 2.5))"
			"    y)",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 0);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) x))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.3, result.data.number, 0);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((x 2.3)) y))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 0);

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"(let ((x 1.6)"
			"      (y 2.5))"
			"    (let ((test-capture (fn () x)))"
			"      (let ((x 4)) (test-capture))))",
			&result));
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(2.5, result.data.number, 0);

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
