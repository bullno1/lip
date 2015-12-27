#ifndef LIP_VM_H
#define LIP_VM_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "value.h"
#include "function.h"

typedef struct lip_linker_t lip_linker_t;

typedef struct lip_vm_context_t
{
	lip_closure_t* closure;
	lip_instruction_t* pc;
	lip_value_t* ep;
	bool is_native;
} lip_vm_context_t;

typedef struct lip_vm_t
{
	lip_vm_context_t ctx;

	lip_value_t* min_sp;
	lip_value_t* max_sp;
	lip_value_t* min_ep;
	lip_value_t* max_ep;
	lip_vm_context_t* min_fp;
	lip_vm_context_t* max_fp;

	lip_value_t* sp;
	lip_vm_context_t* fp;

	lip_linker_t* linker;
} lip_vm_t;

size_t lip_vm_config(
	lip_vm_t* vm,
	lip_linker_t* linker,
	uint16_t operand_stack_size,
	uint16_t environment_size,
	uint16_t call_stack_size
);

void lip_vm_init(lip_vm_t* vm, void* mem);

void lip_vm_push_nil(lip_vm_t* vm);
void lip_vm_push_number(lip_vm_t* vm, double number);
void lip_vm_push_value(lip_vm_t* vm, lip_value_t* value);
lip_exec_status_t lip_vm_call(lip_vm_t* vm, uint8_t num_args);
lip_value_t* lip_vm_pop(lip_vm_t* vm);
lip_value_t* lip_vm_get_arg(lip_vm_t* vm, uint16_t index);

#endif
