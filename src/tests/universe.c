#include <lip/runtime.h>
#include <lip/memory.h>
#include <lip/arena_allocator.h>
#include "munit.h"
#include "test_helpers.h"

typedef struct lip_fixture_s lip_fixture_t;

struct lip_fixture_s
{
	lip_allocator_t* arena_allocator;
	lip_runtime_t* runtime;
};

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	lip_fixture_t* fixture = lip_new(lip_default_allocator, lip_fixture_t);
	lip_allocator_t* arena_allocator = lip_arena_allocator_create(lip_default_allocator, 1024);
	lip_vm_config_t vm_config = {
		.allocator = arena_allocator,
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	fixture->arena_allocator = arena_allocator;
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
	lip_arena_allocator_destroy(fixture->arena_allocator);
	lip_free(lip_default_allocator, fixture);
}

static MunitResult
ns(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_runtime_t* runtime = fixture->runtime;

	lip_value_t result;
	const lip_error_t* error;

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (ns 1 a) ",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 3 },
		.end = { .line = 1, .column = 10 }
	}));

	munit_assert_false(
		lip_runtime_exec_string(
			runtime,
			"  (ns 1) ",
			&result));
	error = lip_runtime_last_error(runtime);
	lip_assert_enum(lip_error_stage_t, error->code, ==, LIP_STAGE_COMPILER);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 3 },
		.end = { .line = 1, .column = 8 }
	}));

	munit_assert_true(
		lip_runtime_exec_string(
			runtime,
			"  (ns foo) ",
			&result));

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/ns",
		.test = ns,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite universe = {
	.prefix = "/universe",
	.tests = tests
};
