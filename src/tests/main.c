#include <stdio.h>
#include <stdlib.h>
#include "greatest.h"

#define FOREACH_SUITE(F) \
	F(array)

#define DECLARE_SUITE(S) extern SUITE(S);
#define EXEC_SUITE(S) RUN_SUITE(S);

FOREACH_SUITE(DECLARE_SUITE)

GREATEST_MAIN_DEFS();

int main(int argc, char* argv[])
{
	GREATEST_MAIN_BEGIN();
	FOREACH_SUITE(EXEC_SUITE);
	GREATEST_MAIN_END();
	return EXIT_SUCCESS;
}
