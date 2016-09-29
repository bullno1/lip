#ifndef LIP_PARSER_H
#define LIP_PARSER_H

#include "common.h"
#include "sexp.h"
#include "lexer.h"

#define LIP_PARSE_ERROR(F) \
	F(LIP_PARSE_LEX_ERROR) \
	F(LIP_PARSE_UNEXPECTED_TOKEN) \
	F(LIP_PARSE_UNTERMINATED_LIST)

LIP_ENUM(lip_parse_error_t, LIP_PARSE_ERROR)

typedef struct lip_parser_s lip_parser_t;

lip_parser_t*
lip_parser_create(lip_allocator_t* allocator);

void
lip_parser_destroy(lip_parser_t* parser);

void
lip_parser_reset(lip_parser_t* parser, lip_in_t* input);

lip_stream_status_t
lip_parser_next_sexp(lip_parser_t* parser, lip_sexp_t* sexp);

lip_error_t*
lip_parser_last_error(lip_parser_t* parser);

#endif
