#ifndef LIP_LEXER_H
#define LIP_LEXER_H

#include "extra.h"

#define LIP_TOKEN(F) \
	F(LIP_TOKEN_LPAREN) \
	F(LIP_TOKEN_RPAREN) \
	F(LIP_TOKEN_SYMBOL) \
	F(LIP_TOKEN_STRING) \
	F(LIP_TOKEN_NUMBER)

LIP_ENUM(lip_token_type_t, LIP_TOKEN)

#define LIP_LEX_ERROR(F) \
	F(LIP_LEX_BAD_STRING) \
	F(LIP_LEX_BAD_NUMBER)

LIP_ENUM(lip_lex_error_t, LIP_LEX_ERROR)

typedef struct lip_token_s lip_token_t;
typedef struct lip_lexer_s lip_lexer_t;

struct lip_token_s
{
	lip_token_type_t type;
	lip_string_ref_t lexeme;
	lip_loc_range_t location;
};

struct lip_lexer_s
{
	lip_allocator_t* allocator;
	lip_last_error_t last_error;
	lip_in_t* input;
	lip_loc_t location;
	lip_array(lip_array(char)) capture_buffs;
	lip_array(char) capture_buff;
	char read_buff;
	bool capturing;
	bool buffered;
	bool eos;
};

void
lip_lexer_init(lip_lexer_t* lexer, lip_allocator_t* allocator);

void
lip_lexer_cleanup(lip_lexer_t* lexer);

void
lip_lexer_reset(lip_lexer_t* lexer, lip_in_t* input);

lip_stream_status_t
lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token);

const lip_error_t*
lip_lexer_last_error(lip_lexer_t* lexer);

#endif
