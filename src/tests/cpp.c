#include "munit.h"

#ifdef LIP_TEST_SKIP_CPP

static MunitResult
test(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	return MUNIT_SKIP;
}

#else

void test_cpp(void);

static MunitResult
test(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	test_cpp();

	return MUNIT_OK;
}

#endif

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
