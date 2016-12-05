#ifndef LIP_LEXER_EX_H
#define LIP_LEXER_EX_H

#include "../lexer.h"

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

#endif
