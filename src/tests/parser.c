#include <lip/parser.h>
#include <lip/lexer.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/array.h>
#include "munit.h"
#include "test_helpers.h"

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	return lip_parser_create(lip_default_allocator);
}

static void
teardown(void* data)
{
	lip_parser_destroy(data);
}

#define LIP_LIST(...) \
	{ .list = (lip_sexp_t[]) { \
		__VA_ARGS__, \
		{ .type = LIP_SEXP_LIST, .data = { .list = 0 }} \
	}}

#define LIP_EMPTY_LIST \
	{ .list = (lip_sexp_t[]) { \
		{ .type = LIP_SEXP_LIST, .data = { .list = 0 }} \
	}}

static void
lip_assert_sexp_equal(lip_sexp_t lhs, lip_sexp_t rhs);

static void
lip_assert_sexp_list_equal(lip_sexp_t* expected, lip_array(lip_sexp_t) actual)
{
	unsigned int len = 0;
	while(!(expected[len].type == LIP_SEXP_LIST
			&& expected[len].data.list == NULL)) //special "end" marker
	{
		++len;
	}

	munit_assert_size(len, ==, lip_array_len(actual));
	for(unsigned int i = 0; i < len; ++i)
	{
		lip_assert_sexp_equal(expected[i], actual[i]);
	}
}

static void
lip_assert_sexp_equal(lip_sexp_t lhs, lip_sexp_t rhs)
{
	munit_logf(
		MUNIT_LOG_INFO, "%s (%u:%u - %u:%u)",
		lip_sexp_type_t_to_str(rhs.type),
		rhs.location.start.line, rhs.location.start.column,
		rhs.location.end.line, rhs.location.end.column
	);

	lip_assert_enum(lip_sexp_type_t, lhs.type, ==, rhs.type);
	lip_assert_loc_range_equal(lhs.location, rhs.location);
	if(lhs.type == LIP_SEXP_LIST)
	{
		lip_assert_sexp_list_equal(lhs.data.list, rhs.data.list);
	}
	else
	{
		lip_assert_string_ref_equal(lhs.data.string, rhs.data.string);
	}
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref(
		"6(\n"
		"  a->b -4 (\"wat\"() \r\n"
		";( )\n"
		"(5 d)) \"string\")\n"
		"()(\n"
		")\n"
		"'\"ab\"\n"
		"`(cd ,@3 ,(e f))\n"
	);

	struct lip_isstream_s sstream;
	lip_in_t* input = lip_make_isstream(text, &sstream);

	lip_parser_t* parser = fixture;
	lip_parser_reset(parser, input);

	lip_sexp_t expected_sexps[] = {
		{
			.type = LIP_SEXP_NUMBER,
			.location = {
				.start = { .line = 1, .column = 1 },
				.end = { .line = 1, .column = 1 }
			},
			.data = { .string = lip_string_ref("6") },
		},
		{
			.type = LIP_SEXP_LIST,
			.location = {
				.start = { .line = 1, .column = 2 },
				.end = { .line = 4, .column = 16 }
			},
			.data = LIP_LIST(
				{
					.type = LIP_SEXP_SYMBOL,
					.location = {
						.start = { .line = 2, .column = 3 },
						.end = { .line = 2, .column = 6 }
					},
					.data = { .string = lip_string_ref("a->b") }
				},
				{
					.type = LIP_SEXP_NUMBER,
					.location = {
						.start = { .line = 2, .column = 8 },
						.end = { .line = 2, .column = 9 }
					},
					.data = { .string = lip_string_ref("-4") }
				},
				{
					.type = LIP_SEXP_LIST,
					.location = {
						.start = { .line = 2, .column = 11 },
						.end = { .line = 4, .column = 6 }
					},
					.data = LIP_LIST(
						{
							.type = LIP_SEXP_STRING,
							.location = {
								.start = { .line = 2, .column = 12 },
								.end = { .line = 2, .column = 16 }
							},
							.data = { .string = lip_string_ref("wat") }
						},
						{
							.type = LIP_SEXP_LIST,
							.location = {
								.start = { .line = 2, .column = 17 },
								.end = { .line = 2, .column = 18 }
							},
							.data = LIP_EMPTY_LIST
						},
						{
							.type = LIP_SEXP_LIST,
							.location = {
								.start = { .line = 4, .column = 1},
								.end = { .line = 4, .column = 5}
							},
							.data = LIP_LIST(
								{
									.type = LIP_SEXP_NUMBER,
									.location = {
										.start = { .line = 4, .column = 2},
										.end = { .line = 4, .column = 2}
									},
									.data = { .string = lip_string_ref("5") }
								},
								{
									.type = LIP_SEXP_SYMBOL,
									.location = {
										.start = { .line = 4, .column = 4},
										.end = { .line = 4, .column = 4}
									},
									.data = { .string = lip_string_ref("d") }
								}
							)
						}
					)
				},
				{
					.type = LIP_SEXP_STRING,
					.location = {
						.start = { .line = 4, .column = 8 },
						.end = { .line = 4, .column = 15 }
					},
					.data = { .string = lip_string_ref("string") }
				}
			)
		},
		{
			.type = LIP_SEXP_LIST,
			.location = {
				.start = { .line = 5, .column = 1 },
				.end = { .line = 5, .column = 2 }
			},
			.data = LIP_EMPTY_LIST
		},
		{
			.type = LIP_SEXP_LIST,
			.location = {
				.start = { .line = 5, .column = 3 },
				.end = { .line = 6, .column = 1 }
			},
			.data = LIP_EMPTY_LIST
		},
		{
			.type = LIP_SEXP_LIST,
			.location = {
				.start = { .line = 7, .column = 1 },
				.end = { .line = 7, .column = 5 }
			},
			.data = LIP_LIST(
				{
					.type = LIP_SEXP_SYMBOL,
					.location = {
						.start = { .line = 7, .column = 1 },
						.end = { .line = 7, .column = 1 }
					},
					.data = { .string = lip_string_ref("quote") }
				},
				{
					.type = LIP_SEXP_STRING,
					.location = {
						.start = { .line = 7, .column = 2 },
						.end = { .line = 7, .column = 5 }
					},
					.data = { .string = lip_string_ref("ab") }
				}
			)
		},
		{
			.type = LIP_SEXP_LIST,
			.location = {
				.start = { .line = 8, .column = 1 },
				.end = { .line = 8, .column = 16 }
			},
			.data = LIP_LIST(
				{
					.type = LIP_SEXP_SYMBOL,
					.location = {
						.start = { .line = 8, .column = 1 },
						.end = { .line = 8, .column = 1 }
					},
					.data = { .string = lip_string_ref("quasiquote") }
				},
				{
					.type = LIP_SEXP_LIST,
					.location = {
						.start = { .line = 8, .column = 2 },
						.end = { .line = 8, .column = 16}
					},
					.data = LIP_LIST(
						{
							.type = LIP_SEXP_SYMBOL,
							.location = {
								.start = { .line = 8, .column = 3 },
								.end = { .line = 8, .column = 4 }
							},
							.data = { .string = lip_string_ref("cd") }
						},
						{
							.type = LIP_SEXP_LIST,
							.location = {
								.start = { .line = 8, .column = 6 },
								.end = { .line = 8, .column = 8 }
							},
							.data = LIP_LIST(
								{
									.type = LIP_SEXP_SYMBOL,
									.location = {
										.start = { .line = 8, .column = 6 },
										.end = { .line = 8, .column = 7 }
									},
									.data = { .string = lip_string_ref("unquote-splicing") }
								},
								{
									.type = LIP_SEXP_NUMBER,
									.location = {
										.start = { .line = 8, .column = 8 },
										.end = { .line = 8, .column = 8 }
									},
									.data = { .string = lip_string_ref("3") }
								}
							)
						},
						{
							.type = LIP_SEXP_LIST,
							.location = {
								.start = { .line = 8, .column = 10 },
								.end = { .line = 8, .column = 15 }
							},
							.data = LIP_LIST(
								{
									.type = LIP_SEXP_SYMBOL,
									.location = {
										.start = { .line = 8, .column = 10 },
										.end = { .line = 8, .column = 10 }
									},
									.data = { .string = lip_string_ref("unquote") }
								},
								{
									.type = LIP_SEXP_LIST,
									.location = {
										.start = { .line = 8, .column = 11 },
										.end = { .line = 8, .column = 15 }
									},
									.data = LIP_LIST(
										{
											.type = LIP_SEXP_SYMBOL,
											.location = {
												.start = { .line = 8, .column = 12 },
												.end = { .line = 8, .column = 12 }
											},
											.data = { .string = lip_string_ref("e") }
										},
										{
											.type = LIP_SEXP_SYMBOL,
											.location = {
												.start = { .line = 8, .column = 14 },
												.end = { .line = 8, .column = 14 }
											},
											.data = { .string = lip_string_ref("f") }
										}
									)
								}
							)
						}
					)
				}
			)
		}
	};

	lip_sexp_t sexp;
	unsigned int num_sexps = LIP_STATIC_ARRAY_LEN(expected_sexps);
	for(unsigned int i = 0; i < num_sexps; ++i)
	{
		lip_sexp_t* expected_sexp = &expected_sexps[i];
		lip_stream_status_t status = lip_parser_next_sexp(parser, &sexp);

		lip_assert_enum(lip_stream_status_t, LIP_STREAM_OK, ==, status);

		lip_assert_sexp_equal(*expected_sexp, sexp);
	}

	lip_assert_enum(lip_stream_status_t, LIP_STREAM_END, ==, lip_parser_next_sexp(parser, &sexp));

	return MUNIT_OK;
}

static MunitResult
unterminated_list(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref("((");

	struct lip_isstream_s sstream;
	lip_in_t* input = lip_make_isstream(text, &sstream);

	lip_parser_t* parser = fixture;
	lip_parser_reset(parser, input);

	lip_sexp_t sexp;
	lip_stream_status_t status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	const lip_error_t* error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_UNTERMINATED_LIST, ==, error->code);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 2 },
		.end = { .line = 1, .column = 2 }
	}));

	text = lip_string_ref("a (()");
	input = lip_make_isstream(text, &sstream);
	lip_parser_reset(parser, input);

	status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_OK, ==, status);
	status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_UNTERMINATED_LIST, ==, error->code);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 3 },
		.end = { .line = 1, .column = 3 }
	}));

	return MUNIT_OK;
}

static MunitResult
unexpected_token(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref("(3 ')");

	struct lip_isstream_s sstream;
	lip_in_t* input = lip_make_isstream(text, &sstream);

	lip_parser_t* parser = fixture;
	lip_parser_reset(parser, input);

	lip_sexp_t sexp;
	lip_stream_status_t status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	const lip_error_t* error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_UNEXPECTED_TOKEN, ==, error->code);
	const lip_token_t* token = error->extra;
	lip_assert_enum(lip_token_type_t, LIP_TOKEN_RPAREN, ==, token->type);
	lip_assert_string_ref_equal(lip_string_ref(")"), token->lexeme);
	lip_assert_loc_range_equal(error->location, token->location);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 5 },
		.end = { .line = 1, .column = 5 }
	}));

	text = lip_string_ref(" )");
	input = lip_make_isstream(text, &sstream);
	lip_parser_reset(parser, input);

	status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_UNEXPECTED_TOKEN, ==, error->code);
	token = error->extra;
	lip_assert_enum(lip_token_type_t, LIP_TOKEN_RPAREN, ==, token->type);
	lip_assert_string_ref_equal(lip_string_ref(")"), token->lexeme);
	lip_assert_loc_range_equal(error->location, token->location);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 2 },
		.end = { .line = 1, .column = 2 }
	}));

	text = lip_string_ref(" '");
	input = lip_make_isstream(text, &sstream);
	lip_parser_reset(parser, input);

	status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_UNEXPECTED_TOKEN, ==, error->code);
	token = error->extra;
	lip_assert_enum(lip_token_type_t, LIP_TOKEN_QUOTE, ==, token->type);
	lip_assert_string_ref_equal(lip_string_ref("'"), token->lexeme);
	lip_assert_loc_range_equal(error->location, token->location);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 2 },
		.end = { .line = 1, .column = 2 }
	}));

	return MUNIT_OK;
}

static MunitResult
lex_error(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_string_ref_t text = lip_string_ref("5..6");

	struct lip_isstream_s sstream;
	lip_in_t* input = lip_make_isstream(text, &sstream);

	lip_parser_t* parser = fixture;
	lip_parser_reset(parser, input);

	lip_sexp_t sexp;
	lip_stream_status_t status = lip_parser_next_sexp(parser, &sexp);
	lip_assert_enum(lip_stream_status_t, LIP_STREAM_ERROR, ==, status);
	const lip_error_t* error = lip_parser_last_error(parser);
	lip_assert_enum(lip_parse_error_t, LIP_PARSE_LEX_ERROR, ==, error->code);
	const lip_error_t* lex_error = error->extra;
	lip_assert_enum(lip_lex_error_t, LIP_LEX_BAD_NUMBER, ==, lex_error->code);
	lip_assert_loc_range_equal(error->location, lex_error->location);
	lip_assert_loc_range_equal(error->location, ((lip_loc_range_t){
		.start = { .line = 1, .column = 1 },
		.end = { .line = 1, .column = 3 }
	}));

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
		.name = "/unterminated_list",
		.test = unterminated_list,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/unexpected_token",
		.test = unexpected_token,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/lex_error",
		.test = lex_error,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite parser = {
	.prefix = "/parser",
	.tests = tests
};
