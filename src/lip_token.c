#include "lip_token.h"
#include <stdio.h>

void lip_token_print(lip_token_t* token)
{
	printf(
		"%s '%.*s' %u:%u - %u:%u",
		lip_token_type_t_to_str(token->type),
		(int)token->length, token->lexeme,
		token->start.line, token->start.column,
		token->end.line, token->end.column
	);
}
