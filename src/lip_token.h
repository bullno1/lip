#ifndef LIP_TOKEN_H
#define LIP_TOKEN_H

#include <stdlib.h>
#include "lip_types.h"
#include "lip_enum.h"

#define LIP_TOKEN(F) \
	F(LIP_TOKEN_LPAREN) \
	F(LIP_TOKEN_RPAREN) \
	F(LIP_TOKEN_SEMICOLON) \
	F(LIP_TOKEN_SYMBOL) \
	F(LIP_TOKEN_STRING) \
	F(LIP_TOKEN_NUMBER) \
	F(LIP_TOKEN_COUNT)

LIP_ENUM(lip_token_type_t, LIP_TOKEN)

typedef struct
{
	lip_token_type_t type;
	const char* lexeme;
	size_t length;
	lip_loc_t start;
	lip_loc_t end;
} lip_token_t;

void lip_token_print(lip_token_t* token);

#endif
