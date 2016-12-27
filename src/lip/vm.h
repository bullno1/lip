#ifndef LIP_VM_H
#define LIP_VM_H

#include "common.h"

typedef struct lip_vm_config_s lip_vm_config_t;
typedef struct lip_vm_hook_s lip_vm_hook_t;

struct lip_value_s
{
	lip_value_type_t type;
	union
	{
		uint32_t index;
		void* reference;
		bool boolean;
		double number;
	} data;
};

struct lip_vm_config_s
{
	uint32_t os_len;
	uint32_t cs_len;
	uint32_t env_len;
};

struct lip_vm_hook_s
{
	void(*step)(lip_vm_hook_t* hook, const lip_vm_t* vm);
};

void
lip_vm_reset(lip_vm_t* vm);

lip_vm_hook_t*
lip_vm_set_hook(lip_vm_t* vm, lip_vm_hook_t* hook);

lip_exec_status_t
lip_vm_call(
	lip_vm_t* vm,
	lip_value_t* result,
	lip_value_t fn,
	unsigned int num_args,
	...
);

lip_value_t*
lip_vm_get_args(const lip_vm_t* vm, uint8_t* num_args);

lip_value_t*
lip_vm_get_env(const lip_vm_t* vm, uint8_t* env_len);

#endif
