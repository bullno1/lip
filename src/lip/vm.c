#include <lip/vm.h>
#include <stdarg.h>
#include "memory.h"
#include "vm_dispatch.h"

size_t
lip_vm_memory_required(lip_vm_config_t* config)
{
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_memblock_info_t mem_layout =
		lip_vm_memory_layout(config, &os_block, &env_block, &cs_block);

	return mem_layout.num_elements;
}

void
lip_reset_vm(lip_vm_t* vm)
{
	lip_vm_init(vm, &vm->config, vm->rt, vm->mem);
}

void
lip_vm_init(
	lip_vm_t* vm, lip_vm_config_t* config, lip_runtime_interface_t* rt, void* mem
)
{
	vm->config = *config;
	vm->rt = rt;
	vm->hook = NULL;
	vm->mem = mem;

	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(config, &os_block, &env_block, &cs_block);
	vm->sp = (lip_value_t*)lip_locate_memblock(mem, &os_block) + config->os_len;
	vm->fp = lip_locate_memblock(mem, &cs_block);
	vm->fp->ep = (lip_value_t*)lip_locate_memblock(mem, &env_block) + config->env_len;
	vm->fp->bp = vm->sp;
	vm->fp->is_native = false;
	vm->fp->pc = NULL;
	vm->fp->closure = NULL;
}

lip_exec_status_t
lip_call(
	lip_vm_t* vm,
	lip_value_t* result,
	lip_value_t fn,
	unsigned int num_args,
	...
)
{
	vm->sp -= num_args;
	va_list args;
	va_start(args, num_args);
	for(unsigned int i = 0; i < LIP_MIN(UINT8_MAX, num_args); ++i)
	{
		vm->sp[i] = va_arg(args, lip_value_t);
	}
	va_end(args);

	lip_stack_frame_t* old_fp = vm->fp;
	vm->fp->is_native = true;
	*(++vm->fp) = *old_fp;

	lip_exec_status_t status = lip_vm_do_call(vm, &fn, num_args);
	if(status != LIP_EXEC_OK) { return status; }

	if(vm->fp == old_fp)
	{
		status = LIP_EXEC_OK;
	}
	else
	{
		status = lip_vm_loop(vm);
	}

	*result = *vm->sp;
	++vm->sp;
	return status;
}

lip_vm_hook_t*
lip_set_vm_hook(lip_vm_t* vm, lip_vm_hook_t* hook)
{
	lip_vm_hook_t* old_hook = vm->hook;
	vm->hook = hook;
	return old_hook;
}

lip_value_t*
lip_get_args(const lip_vm_t* vm, uint8_t* num_args)
{
	if(num_args) { *num_args = vm->fp->num_args; }
	return vm->fp->bp;
}

lip_value_t*
lip_get_env(const lip_vm_t* vm, uint8_t* env_len)
{
	if(env_len) { *env_len = vm->fp->closure->env_len; }
	return vm->fp->closure->environment;
}
