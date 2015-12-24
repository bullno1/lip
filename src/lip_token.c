#include "lip_token.h"
#include <stdio.h>

void lip_token_print(lip_token_t* token)
{
	printf(
		"%d '%.*s' %u:%u - %u:%u",
		token->type,
		(int)token->length, token->lexeme,
		token->start.line, token->start.column,
		token->end.line, token->end.column
	);
}
