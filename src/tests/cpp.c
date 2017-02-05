#include "munit.h"

void test_cpp(void);

static MunitResult
test(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	test_cpp();

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/cpp",
		.test = test,
	},
	{ .test = NULL }
};

MunitSuite cpp = {
	.prefix = "/cpp",
	.tests = tests
};
