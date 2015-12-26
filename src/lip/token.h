#ifndef LIP_TOKEN_H
#define LIP_TOKEN_H

#include <stdlib.h>
#include "types.h"
#include "enum.h"

#define LIP_TOKEN(F) \
	F(LIP_TOKEN_LPAREN) \
	F(LIP_TOKEN_RPAREN) \
	F(LIP_TOKEN_SYMBOL) \
	F(LIP_TOKEN_STRING) \
	F(LIP_TOKEN_NUMBER)

LIP_ENUM(lip_token_type_t, LIP_TOKEN)

typedef struct lip_token_t
{
	lip_token_type_t type;
	lip_string_ref_t lexeme;
	lip_loc_t start;
	lip_loc_t end;
} lip_token_t;

void lip_token_print(lip_token_t* token);

#endif
