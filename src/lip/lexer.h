#ifndef LIP_LEXER_H
#define LIP_LEXER_H

#include <stdbool.h>
#include "types.h"
#include "enum.h"

#define LIP_LEX(F) \
	F(LIP_LEX_OK) \
	F(LIP_LEX_BAD_STRING) \
	F(LIP_LEX_BAD_NUMBER) \
	F(LIP_LEX_EOS)

LIP_ENUM(lip_lex_status_t, LIP_LEX)

typedef struct lip_token_t lip_token_t;

typedef struct lip_lexer_t
{
	lip_allocator_t* allocator;
	lip_loc_t location;
	lip_array(char) capture_buff;
	lip_array(char*) strings;
	char buff;
	bool capturing;
	bool buffered;
	bool eos;

	lip_read_fn_t read_fn;
	void* read_ctx;
} lip_lexer_t;

void lip_lexer_init(
	lip_lexer_t* lexer,
	lip_allocator_t* allocator,
	lip_read_fn_t read_fn,
	void* read_ctx
);
lip_lex_status_t lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token);
void lip_lexer_print_status(lip_lex_status_t status, lip_token_t* token);
void lip_lexer_reset(lip_lexer_t* lexer);
void lip_lexer_cleanup(lip_lexer_t* lexer);

#endif
