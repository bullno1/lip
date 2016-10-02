#include "builtins.h"
#include "memory.h"
#include "ex/vm.h"
#include "io.h"
#include "print.h"

static lip_exec_status_t
lip_print(lip_vm_t* vm)
{
	lip_print_value(
		vm->config.allocator,
		10,
		0,
		lip_stdout(),
		lip_vm_get_arg(vm, 0)
	);
	return LIP_EXEC_OK;
}

static lip_exec_status_t
lip_nop(lip_vm_t* vm)
{
	(void)vm;
	return LIP_EXEC_OK;
}

static lip_exec_status_t
lip_identity(lip_vm_t* vm)
{
	lip_vm_push_value(vm, lip_vm_get_arg(vm, 0));
	return LIP_EXEC_OK;
}

void lip_builtins_open(lip_lni_t* lni)
{
	lip_native_function_t functions[] = {
		LIP_NATIVE_FN(nop, lip_nop, 0),
		LIP_NATIVE_FN(identity, lip_identity, 1),
		LIP_NATIVE_FN(print, lip_print, 1),
	};
	lip_lni_register(
		lni, lip_string_ref(""), functions, LIP_STATIC_ARRAY_LEN(functions)
	);
}
