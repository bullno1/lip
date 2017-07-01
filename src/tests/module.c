#include "munit.h"
#include "runtime_helper.h"
#include <lip/bind.h>

typedef struct lip_test_context_s
{
	int count;
} lip_test_context_t;

static lip_test_context_t test_ctx = { 0 };

static lip_function(count_load)
{
	++test_ctx.count;
	lip_return(lip_make_nil(vm));
}

static MunitResult
basic(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx2 = fixture->context;
	lip_load_builtins(ctx2);

	lip_module_context_t* module = lip_begin_module(ctx2, lip_string_ref("test"));
	lip_declare_function(module, lip_string_ref("count-load"), count_load);
	lip_end_module(ctx2, module);

	test_ctx = (lip_test_context_t){ 0 };

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(test.mod/test-fun 1)", 2, true);
		lip_assert_num_result("(test.mod/test-fun 2)", 3, true);
		lip_destroy_context(ctx);
	}

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(test.mod3/test-fun2 3)", 2, true);
		lip_destroy_context(ctx);
	}

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(test.mod/test-fun 3)", 4, true);
		lip_assert_num_result("(test.mod3/test-fun2 4)", 3, true);
		lip_destroy_context(ctx);
	}
	munit_assert_int(1, ==, test_ctx.count);

	return MUNIT_OK;
}

static MunitResult
local_function(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx2 = fixture->context;
	lip_load_builtins(ctx2);

	lip_module_context_t* module = lip_begin_module(ctx2, lip_string_ref("test"));
	lip_declare_function(module, lip_string_ref("count-load"), count_load);
	lip_end_module(ctx2, module);

	test_ctx = (lip_test_context_t){ 0 };

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(mod4/a 3)", 6, true);
		lip_destroy_context(ctx);
	}

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(mod4/a -2)", -4, true);
		lip_destroy_context(ctx);

		munit_assert_int(1, ==, test_ctx.count);
	}

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_error_msg("(mod5/a 3)", "Undefined symbol: mod5/a");
		lip_destroy_context(ctx);
	}

	return MUNIT_OK;
}

static MunitResult
no_declare_in_body(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx2 = fixture->context;
	lip_load_builtins(ctx2);

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_error_msg("(mod7/b 3)", "Undefined symbol: mod7/b");
		const lip_context_error_t* error = lip_get_error(ctx);
		while(error->parent) { error = error->parent; }
		lip_assert_string_ref_equal(
			lip_string_ref("Cannot use `declare` inside a `declare`-d function"),
			error->message
		);
		lip_destroy_context(ctx);
	}

	return MUNIT_OK;
}

static MunitResult
private_function(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx2 = fixture->context;
	lip_load_builtins(ctx2);

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_num_result("(mod6/a -42)", -42);
		lip_destroy_context(ctx);
	}

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		lip_assert_error_msg("(mod6/b 3)", "Undefined symbol: mod6/b");
		lip_destroy_context(ctx);
	}

	return MUNIT_OK;
}

static MunitResult
error(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx2 = fixture->context;
	lip_load_builtins(ctx2);

	{
		lip_context_t* ctx = lip_create_context(fixture->runtime, NULL);
		lip_vm_t* vm = lip_create_vm(ctx, NULL);
		for(int i = 0; i < 500; ++i)
		{
			lip_assert_error_msg("(mod9/b 3)", "Undefined symbol: mod9/b");
		}
		lip_assert_num_result("(mod6/a -42)", -42);
		lip_destroy_context(ctx);
	}

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/basic",
		.test = basic,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/local_function",
		.test = local_function,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/private_function",
		.test = private_function,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/no_declare_in_body",
		.test = no_declare_in_body,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/error",
		.test = error,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite module = {
	.prefix = "/module",
	.tests = tests
};
