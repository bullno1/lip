#include <lip/lip.h>
#include <math.h>
#include <lip/bind.h>
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

static
lip_function(lip_pow)
{
	lip_bind_args((number, x), (number, y));
	lip_return((lip_make_number(vm, pow(x, y))));
}

static MunitResult
direct(const MunitParameter params[], void* fixture_)
{
	(void)params;
	lip_fixture_t* fixture = fixture_;
	lip_vm_t* vm = fixture->vm;

	lip_value_t fn = lip_make_function(vm, lip_pow, 0, NULL);
	lip_value_t result;
	lip_exec_status_t status =
		lip_call(vm, &result, fn, 2, lip_make_number(vm, 3.5), lip_make_number(vm, 3.6));

	lip_assert_enum(lip_exec_status_t, LIP_EXEC_OK, ==, status);
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(pow(3.5, 3.6), result.data.number, 3);

	return MUNIT_OK;
}

static lip_wrap_function(pow, number, number, number)

static MunitResult
wrapper(const MunitParameter params[], void* fixture_)
{
	(void)params;
	lip_fixture_t* fixture = fixture_;
	lip_vm_t* vm = fixture->vm;

	lip_value_t fn = lip_make_function(vm, lip_wrapper(pow), 0, NULL);
	lip_value_t result;
	lip_exec_status_t status =
		lip_call(vm, &result, fn, 2, lip_make_number(vm, 3.5), lip_make_number(vm, 3.6));

	lip_assert_enum(lip_exec_status_t, LIP_EXEC_OK, ==, status);
	lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(pow(3.5, 3.6), result.data.number, 3);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/direct",
		.test = direct,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/wrapper",
		.test = wrapper,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite bind = {
	.prefix = "/bind",
	.tests = tests
};
