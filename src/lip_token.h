#ifndef LIP_TOKEN_H
#define LIP_TOKEN_H

#include <stdlib.h>

typedef enum
{
	LIP_TOKEN_LPAREN,
	LIP_TOKEN_RPAREN,
	LIP_TOKEN_SEMICOLON,
	LIP_TOKEN_SYMBOL,
	LIP_TOKEN_STRING,
	LIP_TOKEN_NUMBER,
	LIP_TOKEN_ERROR,
	LIP_TOKEN_EOS,

	LIP_TOKEN_COUNT
} lip_token_type_t;

typedef struct
{
	unsigned int line;
	unsigned int column;
} lip_loc_t;

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
