#include "token.h"
#include <stdio.h>
#include "utils.h"

void lip_token_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_token_t* token
)
{
	lip_printf(
		write_fn, ctx,
		"%s '%.*s' %u:%u - %u:%u",
		lip_token_type_t_to_str(token->type) + sizeof("LIP_TOKEN_") - 1,
		(int)token->lexeme.length, token->lexeme.ptr,
		token->start.line, token->start.column,
		token->end.line, token->end.column
	);
}
