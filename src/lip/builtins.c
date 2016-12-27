#include <lip/lip.h>
#include <lip/print.h>
#include <lip/io.h>

static lip_exec_status_t
lip_nop(lip_vm_t* vm, lip_value_t* result)
{
	(void)vm;
	(void)result;
	return LIP_EXEC_OK;
}

static lip_exec_status_t
lip_identity(lip_vm_t* vm, lip_value_t* result)
{
	(void)vm;
	uint8_t argc;
	lip_value_t* argv = lip_get_args(vm, &argc);
	if(argc != 1) { return LIP_EXEC_ERROR; }
	*result = argv[0];

	return LIP_EXEC_OK;
}

static lip_exec_status_t
lip_print(lip_vm_t* vm, lip_value_t* result)
{
	(void)vm;
	(void)result;

	uint8_t argc;
	lip_value_t* argv = lip_get_args(vm, &argc);
	if(argc != 1) { return LIP_EXEC_ERROR; }
	lip_print_value(3, 0, lip_stdout(), argv[0]);

	return LIP_EXEC_OK;
}

void
lip_load_builtins(lip_context_t* ctx)
{
	lip_ns_context_t* ns = lip_begin_ns(ctx, lip_string_ref(""));
	lip_declare_function(ns, lip_string_ref("nop"), lip_nop);
	lip_declare_function(ns, lip_string_ref("identity"), lip_identity);
	lip_declare_function(ns, lip_string_ref("print"), lip_print);
	lip_end_ns(ctx, ns);
}
