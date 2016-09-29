#ifndef LIP_LEXER_H
#define LIP_LEXER_H

#include "common.h"
#include "token.h"

#define LIP_LEX_ERROR(F) \
	F(LIP_LEX_BAD_STRING) \
	F(LIP_LEX_BAD_NUMBER)

LIP_ENUM(lip_lex_error_t, LIP_LEX_ERROR)

typedef struct lip_lexer_s lip_lexer_t;

lip_lexer_t*
lip_lexer_create(lip_allocator_t* allocator);

void
lip_lexer_destroy(lip_lexer_t* lexer);

void
lip_lexer_reset(lip_lexer_t* lexer, lip_in_t* input);

lip_stream_status_t
lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token);

const lip_error_t*
lip_lexer_last_error(lip_lexer_t* lexer);

#endif
