#ifndef LIP_PRINT_H
#define LIP_PRINT_H

#include "common.h"
#include "opcode.h"
#include "ast.h"

LIP_API void
lip_print_instruction(
	lip_out_t* output,
	lip_instruction_t instr
);

LIP_API void
lip_print_value(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_value_t value
);

LIP_API void
lip_print_list(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_list_t* list
);

LIP_API void
lip_print_closure(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_closure_t* closure
);

LIP_API void
lip_print_function(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_function_t* function
);

LIP_API void
lip_print_ast(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_ast_t* ast
);

#endif
