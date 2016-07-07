#include <lip/lexer.h>
#include <lip/memory.h>
#include <lip/io.h>
#include "munit.h"
#include "test_helpers.h"

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	return lip_lexer_create(lip_default_allocator);
}

static void
teardown(void* data)
{
	lip_lexer_destroy(data);
}

static void
lip_assert_token_equal(lip_token_t lhs, lip_token_t rhs)
{
	lip_assert_enum(lip_token_type_t, lhs.type, ==, rhs.type);
	lip_assert_loc_range_equal(lhs.location, rhs.location);
	lip_assert_string_ref_equal(lhs.lexeme, rhs.lexeme);
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref(
		"(\n"
		"  )\n"
		"50.6 \"hi hi \" \r\n"
		"; comment 12 \"hi\" \r\n"
		"\n"
		"test-23 -3 -ve -\r\n"
		"\n"
	);

	struct lip_sstream_s sstream;
	lip_in_t* input = lip_make_sstream(text, &sstream);

	lip_lexer_t* lexer = fixture;
	lip_lexer_reset(lexer, input);

	lip_token_t expected_tokens[] = {
		{
			.type = LIP_TOKEN_LPAREN,
			.lexeme = lip_string_ref("("),
			.location = {
				.start = {.line = 1, .column = 1},
				.end = {.line = 1, .column = 1}
			}
		},
		{
			.type = LIP_TOKEN_RPAREN,
			.lexeme = lip_string_ref(")"),
			.location = {
				.start = {.line = 2, .column = 3},
				.end = {.line = 2, .column = 3},
			}
		},
		{
			.type = LIP_TOKEN_NUMBER,
			.lexeme = lip_string_ref("50.6"),
			.location = {
				.start = {.line = 3, .column = 1},
				.end = {.line = 3, .column = 4}
			}
		},
		{
			.type = LIP_TOKEN_STRING,
			.lexeme = lip_string_ref("hi hi "),
			.location = {
				.start = {.line = 3, .column = 6},
				.end = {.line = 3, .column = 13}
			}
		},
		{
			.type = LIP_TOKEN_SYMBOL,
			.lexeme = lip_string_ref("test-23"),
			.location = {
				.start = {.line = 6, .column = 1},
				.end = {.line = 6, .column = 7}
			}
		},
		{
			.type = LIP_TOKEN_NUMBER,
			.lexeme = lip_string_ref("-3"),
			.location = {
				.start = {.line = 6, .column = 9},
				.end = {.line = 6, .column = 10}
			}
		},
		{
			.type = LIP_TOKEN_SYMBOL,
			.lexeme = lip_string_ref("-ve"),
			.location = {
				.start = {.line = 6, .column = 12},
				.end = {.line = 6, .column = 14}
			}
		},
		{
			.type = LIP_TOKEN_SYMBOL,
			.lexeme = lip_string_ref("-"),
			.location = {
				.start = {.line = 6, .column = 16},
				.end = {.line = 6, .column = 16}
			}
		},
	};

	lip_token_t token;
	unsigned int num_tokens = LIP_STATIC_ARRAY_LEN(expected_tokens);
	for(unsigned int i = 0; i < num_tokens; ++i)
	{
		lip_token_t* expected_token = &expected_tokens[i];
		lip_stream_status_t status = lip_lexer_next_token(lexer, &token);

		munit_logf(
			MUNIT_LOG_INFO, "%s %s '%.*s' (%u:%u - %u:%u)",
			lip_stream_status_t_to_str(status) + sizeof("LIP"),
			lip_token_type_t_to_str(token.type) + sizeof("LIP"),
			(int)token.lexeme.length,
			token.lexeme.ptr,
			token.location.start.line,
			token.location.start.column,
			token.location.end.line,
			token.location.end.column
		);

		lip_assert_enum(lip_stream_status_t, LIP_STREAM_OK, ==, status);
		lip_assert_token_equal(*expected_token, token);
	}

	lip_assert_enum(lip_stream_status_t, LIP_STREAM_END, ==, lip_lexer_next_token(lexer, &token));

	return MUNIT_OK;
}

static MunitResult
bad_string(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref(" \"ha");

	struct lip_sstream_s sstream;
	lip_in_t* input = lip_make_sstream(text, &sstream);

	lip_lexer_t* lexer = fixture;
	lip_lexer_reset(lexer, input);

	lip_token_t token;
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, lip_lexer_next_token(lexer, &token));

	lip_error_t error = {
		.code = LIP_LEX_BAD_STRING,
		.location = {
			.start = { .line = 1, .column = 2},
			.end = { .line = 1, .column = 4}
		}
	};

	lip_assert_error_equal(error, *lip_lexer_last_error(lexer));

	return MUNIT_OK;
}

static MunitResult
bad_number(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref(" 5a");

	struct lip_sstream_s sstream;
	lip_in_t* input = lip_make_sstream(text, &sstream);

	lip_lexer_t* lexer = fixture;
	lip_lexer_reset(lexer, input);

	lip_token_t token;
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, lip_lexer_next_token(lexer, &token));

	lip_error_t error = {
		.code = LIP_LEX_BAD_NUMBER,
		.location = {
			.start = { .line = 1, .column = 2},
			.end = { .line = 1, .column = 3}
		}
	};

	lip_assert_error_equal(error, *lip_lexer_last_error(lexer));

	text = lip_string_ref(" 5..4");
	input = lip_make_sstream(text, &sstream);
	lip_lexer_reset(lexer, input);

	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, lip_lexer_next_token(lexer, &token));
	error = (lip_error_t){
		.code = LIP_LEX_BAD_NUMBER,
		.location = {
			.start = { .line = 1, .column = 2},
			.end = { .line = 1, .column = 4}
		}
	};
	lip_assert_error_equal(error, *lip_lexer_last_error(lexer));

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/normal",
		.test = normal,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/bad_string",
		.test = bad_string,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/bad_number",
		.test = bad_number,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite lexer = {
	.prefix = "/lexer",
	.tests = tests
};
