#include "lip.h"

static lip_exec_status_t
lip_nop(lip_vm_t* vm)
{
	(void)vm;
	return LIP_EXEC_OK;
}

static lip_exec_status_t
lip_identity(lip_vm_t* vm)
{
	(void)vm;
	uint8_t argc;
	lip_value_t* argv = lip_vm_get_args(vm, &argc);
	if(argc != 1) { return LIP_EXEC_ERROR; }
	lip_vm_push_value(vm, argv[0]);

	return LIP_EXEC_OK;
}

void
lip_load_builtins(lip_context_t* ctx)
{
	lip_ns_context_t* ns = lip_begin_ns(ctx, lip_string_ref(""));
	lip_declare_function(ns, lip_string_ref("nop"), lip_nop);
	lip_declare_function(ns, lip_string_ref("identity"), lip_identity);
	lip_end_ns(ctx, ns);
}
