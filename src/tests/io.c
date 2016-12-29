#include <lip/io.h>
#include "munit.h"

static MunitResult
sstream(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	const char* text =
		"first line\n"
		"second line\n"
		;
	lip_string_ref_t ref = lip_string_ref(text);
	struct lip_isstream_s sstream;
	char ch;
	lip_in_t* input = lip_make_isstream(ref, &sstream);
	for(unsigned int i = 0; i < ref.length; ++i)
	{
		munit_assert_size(1, ==, lip_read(&ch, sizeof(ch), input));
		munit_assert_char(text[i], ==, ch);
	}

	munit_assert_size(0, ==, lip_read(&ch, sizeof(ch), input));

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/sstream",
		.test = sstream
	},
	{ .test = NULL }
};

MunitSuite io = {
	.prefix = "/io",
	.tests = tests
};
