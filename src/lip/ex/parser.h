#ifndef LIP_PARSER_EX_H
#define LIP_PARSER_EX_H

#include "../parser.h"
#include "lexer.h"

struct lip_parser_s
{
	lip_allocator_t* allocator;
	lip_last_error_t last_error;
	lip_token_t error_token;
	lip_lexer_t lexer;
	lip_array(lip_array(lip_sexp_t)) lists;
};

#endif
