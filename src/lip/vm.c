#include <lip/vm.h>
#include <lip/lip.h>
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
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(config, &os_block, &env_block, &cs_block);

	*vm = (lip_vm_t){
		.config = *config,
		.rt = rt,
		.mem = mem,
		.sp = (lip_value_t*)lip_locate_memblock(mem, &os_block) + config->os_len,
		.fp = lip_locate_memblock(mem, &cs_block)
	};

	// Clear out debug info
	memset(vm->fp, 0, sizeof(lip_stack_frame_t) * config->cs_len);

	*(vm->fp) = (lip_stack_frame_t){
		.ep = (lip_value_t*)lip_locate_memblock(mem, &env_block) + config->env_len,
		.bp = vm->sp
	};
}

lip_exec_status_t
(lip_call)(
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

	lip_stack_frame_t* old_fp = vm->fp++;
	vm->fp->ep = old_fp->ep;

	lip_exec_status_t status = lip_vm_do_call(vm, &fn, num_args);
	if(status != LIP_EXEC_OK) { goto end; }

	if(vm->fp == old_fp)
	{
		status = LIP_EXEC_OK;
	}
	else
	{
		status = lip_vm_loop(vm);
	}

end:
	*result = *vm->sp;
	++vm->sp;
	return status;
}

void
lip_set_native_location(
	lip_vm_t* vm, const char* function, const char* file, int line
)
{
	vm->fp->native_function = function;
	vm->fp->native_filename = file;
	vm->fp->native_line = line;
}

lip_vm_hook_t*
lip_set_vm_hook(lip_vm_t* vm, lip_vm_hook_t* hook)
{
	lip_vm_hook_t* old_hook = vm->hook;
	vm->hook = hook;
	return old_hook;
}

const lip_value_t*
lip_get_args(const lip_vm_t* vm, uint8_t* num_args)
{
	if(num_args) { *num_args = vm->fp->num_args; }
	return vm->fp->bp;
}

const lip_value_t*
lip_get_env(const lip_vm_t* vm, uint8_t* env_len)
{
	if(env_len) { *env_len = vm->fp->closure->env_len; }
	return vm->fp->closure->environment;
}

lip_value_t
lip_make_string_copy(lip_vm_t* vm, lip_string_ref_t str)
{
	size_t size = sizeof(lip_string_t) + str.length + 1; // null-terminator
	lip_string_t* string = vm->rt->malloc(vm->rt, LIP_VAL_STRING, size);
	string->length = str.length;
	memcpy(string->ptr, str.ptr, str.length);
	string->ptr[str.length] = '\0';
	return (lip_value_t){
		.type = LIP_VAL_STRING,
		.data = { .reference = string }
	};
}


LIP_API lip_value_t
lip_make_function(
	lip_vm_t* vm,
	lip_native_fn_t native_fn,
	uint8_t env_len,
	lip_value_t env[]
)
{
	size_t size = sizeof(lip_closure_t) + sizeof(lip_value_t) * env_len;
	lip_closure_t* closure = vm->rt->malloc(vm->rt, LIP_VAL_FUNCTION, size);
	*closure = (lip_closure_t){
		.is_native = true,
		.env_len = env_len,
		.function = { .native = native_fn }
	};

	if(env_len > 0)
	{
		memcpy(closure->environment, env, sizeof(lip_value_t) * env_len);
	}

	return (lip_value_t){
		.type = LIP_VAL_FUNCTION,
		.data = { .reference = closure }
	};
}
