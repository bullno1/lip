#ifndef LIP_FUNCTION_H
#define LIP_FUNCTION_H

#include <stdint.h>
#include "value.h"
#include "opcode.h"
#include "enum.h"

#define LIP_EXEC(F) \
	F(LIP_EXEC_OK) \
	F(LIP_EXEC_ERROR)

LIP_ENUM(lip_exec_status_t, LIP_EXEC)

typedef struct lip_vm_t lip_vm_t;

typedef lip_exec_status_t (*lip_native_function_t)(lip_vm_t*);

typedef struct lip_function_t lip_function_t;

typedef struct lip_closure_t
{
	struct
	{
		unsigned is_native: 1;
		unsigned native_arity: 7;
	} info;

	union
	{
		lip_function_t* lip;
		lip_native_function_t native;
	} function_ptr;
	lip_value_t environment[];
} lip_closure_t;

typedef struct lip_function_t
{
	uint8_t arity;
	uint8_t stack_size;

	uint16_t num_instructions;
	uint16_t num_constants;
	uint16_t num_functions;
	uint16_t num_imports;

	lip_instruction_t* instructions;
	lip_value_t* constants;
	lip_function_t** functions;
	lip_string_t** import_symbols;
	lip_value_t* import_values;
} lip_function_t;

void lip_function_print(lip_function_t* function, int indent);
void lip_closure_print(lip_closure_t* function, int indent);

#endif
