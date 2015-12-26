#ifndef LIP_LEXER_H
#define LIP_LEXER_H

#include <stdbool.h>
#include "token.h"
#include "enum.h"

#define LIP_LEX(F) \
	F(LIP_LEX_OK) \
	F(LIP_LEX_BAD_STRING) \
	F(LIP_LEX_BAD_NUMBER) \
	F(LIP_LEX_EOS)

LIP_ENUM(lip_lex_status_t, LIP_LEX)

typedef struct lip_lexer_t
{
	const char* buff;
	const char* end;
	lip_loc_t location;
} lip_lexer_t;

void lip_lexer_init(lip_lexer_t* lexer, const char* buff, size_t len);
lip_lex_status_t lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token);
void lip_lexer_print_status(lip_lex_status_t status, lip_token_t* token);

#endif
