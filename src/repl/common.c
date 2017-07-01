#include "common.h"
#include <lip/io.h>

bool
repl_run_script(
	lip_context_t* ctx,
	lip_vm_t* vm,
	lip_string_ref_t filename,
	lip_in_t* input
)
{
	lip_script_t* script = lip_load_script(ctx, filename, input);
	if(!script)
	{
		lip_print_error(lip_stderr(), ctx);
		return false;
	}

	lip_value_t result;
	bool success = lip_exec_script(vm, script, &result) == LIP_EXEC_OK;
	if(!success) { lip_print_error(lip_stderr(), ctx); }
	lip_unload_script(ctx, script);

	return success;
}
