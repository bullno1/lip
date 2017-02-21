#ifndef LIP_PARSER_H
#define LIP_PARSER_H

#include "extra.h"
#include "sexp.h"
#include "lexer.h"

#define LIP_PARSE_ERROR(F) \
	F(LIP_PARSE_LEX_ERROR) \
	F(LIP_PARSE_UNEXPECTED_TOKEN) \
	F(LIP_PARSE_UNTERMINATED_LIST)

LIP_ENUM(lip_parse_error_t, LIP_PARSE_ERROR)

typedef struct lip_parser_s lip_parser_t;

struct lip_parser_s
{
	lip_allocator_t* allocator;
	lip_allocator_t* arena_allocator;
	lip_last_error_t last_error;
	lip_token_t token;
	lip_stream_status_t lexer_status;
	lip_lexer_t lexer;
	bool buffered;
};

LIP_API void
lip_parser_init(lip_parser_t* parser, lip_allocator_t* allocator);

LIP_API void
lip_parser_cleanup(lip_parser_t* parser);

LIP_API void
lip_parser_reset(lip_parser_t* parser, lip_in_t* input);

LIP_API lip_stream_status_t
lip_parser_next_sexp(lip_parser_t* parser, lip_sexp_t* sexp);

LIP_API const lip_error_t*
lip_parser_last_error(lip_parser_t* parser);

#endif
