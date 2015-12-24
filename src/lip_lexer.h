#ifndef LIP_LEXER_H
#define LIP_LEXER_H

#include <stdbool.h>
#include "lip_token.h"

typedef struct
{
	const char* buff;
	const char* end;
	lip_loc_t location;
} lip_lexer_t;

void lip_lexer_init(lip_lexer_t* lexer, const char* buff, size_t len);
bool lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token);

#endif
