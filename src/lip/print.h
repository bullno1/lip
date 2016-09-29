#ifndef LIP_PRINT_H
#define LIP_PRINT_H

#include "common.h"
#include "asm.h"

void
lip_print_instruction(
	lip_allocator_t* allocator,
	lip_out_t* output,
	lip_instruction_t instr
);

void
lip_print_value(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_value_t value
);

void
lip_print_closure(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_closure_t* closure
);

void
lip_print_function(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_function_t* function
);

#endif
