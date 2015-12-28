#include <string.h>
#include "vm.h"
#include "asm.h"

void lip_vm_do_call(lip_vm_t* vm, uint8_t num_args)
{
	lip_value_t* value = --vm->sp;
	lip_closure_t* closure = (lip_closure_t*)value->data.reference;

	bool is_native = closure->info.is_native;
	lip_function_t* lip_function = closure->function_ptr.lip;
	size_t stack_size = is_native ? num_args : lip_function->stack_size;

	// Pop arguments from operand stack into environment
	vm->ctx.ep += stack_size;
	vm->sp -= num_args;
	memcpy(
		vm->ctx.ep - num_args,
		vm->sp,
		num_args * sizeof(lip_value_t)
	);

	vm->ctx.is_native = is_native;
	if(is_native)
	{
		closure->function_ptr.native(vm);
		vm->ctx = *(--vm->fp);
	}
	else
	{
		vm->ctx.pc = lip_function->instructions;
		vm->ctx.closure = closure;
	}
}

lip_exec_status_t lip_vm_loop_with_hook(lip_vm_t* vm)
{
#define USE_HOOK 1
#include "vm_dispatch.inl"
#undef USE_HOOK
}

lip_exec_status_t lip_vm_loop_without_hook(lip_vm_t* vm)
{
#define USE_HOOK 0
#include "vm_dispatch.inl"
#undef USE_HOOK
}

lip_exec_status_t lip_vm_loop(lip_vm_t* vm)
{
	if(vm->hook)
	{
		return lip_vm_loop_with_hook(vm);
	}
	else
	{
		return lip_vm_loop_without_hook(vm);
	}
}
