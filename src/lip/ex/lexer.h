#ifndef LIP_LEXER_EX_H
#define LIP_LEXER_EX_H

#include "../lexer.h"

struct lip_lexer_s
{
	lip_allocator_t* allocator;
	lip_error_t error;
	lip_in_t* input;
	lip_loc_t location;
	lip_array(char) capture_buff;
	lip_array(char*) strings;
	char buff;
	bool capturing;
	bool buffered;
	bool eos;
};

void
lip_lexer_init(lip_lexer_t* lexer, lip_allocator_t* allocator);

void
lip_lexer_cleanup(lip_lexer_t* lexer);

#endif
