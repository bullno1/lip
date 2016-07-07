#ifndef LIP_VM_H
#define LIP_VM_H

#include "common.h"

typedef struct lip_vm_config_s lip_vm_config_t;
typedef struct lip_vm_hook_s lip_vm_hook_t;
typedef void(*lip_vm_hook_fn_t)(lip_vm_hook_t* hook, lip_vm_t* vm);

struct lip_value_s
{
	lip_value_type_t type;
	union
	{
		int64_t index;
		void* reference;
		bool boolean;
		double number;
	} data;
};

struct lip_vm_config_s
{
	lip_allocator_t* allocator;
	uint32_t operand_stack_length;
	uint32_t call_stack_length;
	uint32_t environment_length;
};

struct lip_vm_hook_t
{
	lip_vm_hook_fn_t hook_fn;
};

lip_vm_t*
lip_vm_create(lip_allocator_t* allocator, lip_vm_config_t* config);

void
lip_vm_destroy(lip_vm_t* vm);

lip_vm_hook_t*
lip_vm_set_hook(lip_vm_t* vm, lip_vm_hook_t* hook);

void
lip_vm_push_value(lip_vm_t* vm, lip_value_t value);

lip_exec_status_t
lip_vm_call(lip_vm_t* vm, uint8_t num_args, lip_value_t* result);

lip_value_t
lip_vm_get_arg(lip_vm_t* vm, uint8_t index);

static inline void
lip_vm_push_nil(lip_vm_t* vm)
{
	lip_vm_push_value(vm, (lip_value_t){
		.type = LIP_VAL_NIL
	});
}

static inline void
lip_vm_push_number(lip_vm_t* vm, double number)
{
	lip_vm_push_value(vm, (lip_value_t){
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	});
}

static inline void
lip_vm_push_boolean(lip_vm_t* vm, bool boolean)
{
	lip_vm_push_value(vm, (lip_value_t){
		.type = LIP_VAL_BOOLEAN,
		.data = { .boolean = boolean }
	});
}

#endif
