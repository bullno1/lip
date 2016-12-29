#include <stdio.h>
#include <stdlib.h>
#include "munit.h"

#define FOREACH_SUITE(F) \
	F(io) \
	F(array) \
	F(lexer) \
	F(parser) \
	F(assembler) \
	F(arena_allocator) \
	F(vm) \
	F(runtime) \
	F(bind)

#define DECLARE_SUITE(S) extern MunitSuite S;

#define IMPORT_SUITE(S) S,

FOREACH_SUITE(DECLARE_SUITE)

int
main(int argc, char* argv[])
{
	MunitSuite all_suites[] = {
		FOREACH_SUITE(IMPORT_SUITE)
		{ .tests = NULL, .suites = NULL }
	};

	MunitSuite main_suite = {
		.prefix = "",
		.suites = all_suites
	};

	return munit_suite_main(&main_suite, NULL, argc, argv);
}
