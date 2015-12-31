#include "vm.h"
#include <string.h>
#include "vm_dispatch.h"

size_t lip_vm_config(lip_vm_t* vm, lip_vm_config_t* config)
{
	vm->config = *config;

	return
		  sizeof(lip_value_t) * config->operand_stack_length
		+ sizeof(lip_value_t) * config->environment_length
		+ sizeof(lip_vm_context_t) * config->call_stack_length;
}

void lip_vm_init(lip_vm_t* vm, void* mem)
{
	memset(&vm->ctx, 0, sizeof(lip_vm_context_t));
	vm->hook = NULL;
	vm->hook_ctx = NULL;

	char* ptr = (char*)mem;
	vm->sp = vm->min_sp = (lip_value_t*)ptr;

	ptr += sizeof(lip_value_t*) * vm->config.operand_stack_length;
	vm->ctx.ep = vm->min_ep = vm->max_sp = (void*)ptr;

	ptr += sizeof(lip_value_t*) * vm->config.environment_length;
	vm->max_ep = (void*)ptr;
	vm->fp = vm->min_fp = (void*)ptr;
	vm->max_fp = (void*)(ptr + vm->config.call_stack_length);
}

void lip_vm_push_nil(lip_vm_t* vm)
{
	lip_value_t value = { .type = LIP_VAL_NIL };
	lip_vm_push_value(vm, &value);
}

void lip_vm_push_number(lip_vm_t* vm, double number)
{
	lip_value_t value = {
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	};
	lip_vm_push_value(vm, &value);
}

void lip_vm_push_boolean(lip_vm_t* vm, bool boolean)
{
	lip_value_t value = {
		.type = LIP_VAL_BOOLEAN,
		.data = { .boolean = boolean }
	};
	lip_vm_push_value(vm, &value);
}

void lip_vm_push_value(lip_vm_t* vm, lip_value_t* value)
{
	*(vm->sp++) = *value;
}

lip_exec_status_t lip_vm_call(lip_vm_t* vm, uint8_t num_args)
{
	void* old_fp = vm->fp;
	vm->ctx.is_native = true;
	*(vm->fp++) = vm->ctx;

	lip_vm_do_call(vm, num_args);
	if(vm->fp == old_fp)
	{
		return LIP_EXEC_OK;
	}
	else
	{
		return lip_vm_loop(vm);
	}
}

lip_value_t* lip_vm_pop(lip_vm_t* vm)
{
	return --vm->sp;
}

lip_value_t* lip_vm_get_arg(lip_vm_t* vm, uint8_t index)
{
	return vm->ctx.ep - (int32_t)index - 1;
}
