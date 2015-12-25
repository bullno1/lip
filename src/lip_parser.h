#ifndef LIP_PARSER_H
#define LIP_PARSER_H

#include <stdbool.h>
#include "lip_sexp.h"
#include "lip_token.h"
#include "lip_lexer.h"
#include "lip_enum.h"

#define LIP_PARSE(F) \
	F(LIP_PARSE_OK) \
	F(LIP_PARSE_LEX_ERROR) \
	F(LIP_PARSE_UNEXPECTED_TOKEN) \
	F(LIP_PARSE_UNTERMINATED_LIST) \
	F(LIP_PARSE_EOS)

LIP_ENUM(lip_parse_status_t, LIP_PARSE)

typedef struct lip_allocator_t lip_allocator_t;

typedef struct lip_parser_t
{
	lip_lexer_t* lexer;
	lip_allocator_t* allocator;
} lip_parser_t;

typedef union lip_parse_error_t
{
	struct
	{
		lip_lex_status_t status;
		lip_token_t token;
	} lex_error;
	lip_token_t unexpected_token;
	lip_loc_t location;
} lip_parse_error_t;

typedef union lip_parse_result_t
{
	lip_parse_error_t error;
	lip_sexp_t sexp;
} lip_parse_result_t;

void lip_parser_init(
	lip_parser_t* parser, lip_lexer_t* lexer, lip_allocator_t* allocator
);
lip_parse_status_t lip_parser_next_sexp(
	lip_parser_t* parser, lip_parse_result_t* result
);
void lip_parser_print_status(
	lip_parse_status_t status, lip_parse_result_t* result
);

#endif
