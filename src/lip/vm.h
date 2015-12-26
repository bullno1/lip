#ifndef LIP_VM_H
#define LIP_VM_H

#include <stdint.h>
#include <stdbool.h>
#include "value.h"
#include "function.h"

typedef struct lip_vm_context_t
{
	lip_closure_t* closure;
	uint16_t pc;
	uint16_t sp;
	uint16_t ep;
} lip_vm_context_t;

typedef struct lip_vm_t
{
	lip_vm_context_t context;

	uint16_t num_symbols;
	const char** symbols;

	uint16_t operand_stack_size;
	lip_value_t* operand_stack;

	uint16_t environment_size;
	lip_value_t* environment;

	uint16_t call_stack_size;
	lip_vm_context_t* call_stack;
} lip_vm_t;

lip_exec_status_t lip_vm_execute(lip_function_t* function, lip_value_t* result);
void lip_vm_push(lip_vm_t* vm, lip_value_t* value);
void lip_vm_push_number(lip_vm_t* vm, double value);
void lip_vm_push_boolean(lip_vm_t* vm, bool value);
void lip_vm_push_string(lip_vm_t* vm, const char* value);
void lip_vm_register_native_function(
	lip_vm_t* vm,
	uint8_t arity,
	lip_native_function_t func_ptr,
	uint16_t environment_size,
	lip_value_t* environment
);

#endif
