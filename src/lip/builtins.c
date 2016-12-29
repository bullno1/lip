#include <lip/lip.h>
#include <lip/print.h>
#include <lip/io.h>
#include <lip/bind.h>

static lip_function(nop)
{
	(void)vm;
	(void)result;
	return LIP_EXEC_OK;
}

static lip_function(identity)
{
	lip_bind_args((any, x));
	lip_return(x);
}

static lip_function(print)
{
	lip_bind_args((any, x));
	lip_print_value(3, 0, lip_stdout(), x);
	lip_return(lip_make_nil(vm));
}

void
lip_load_builtins(lip_context_t* ctx)
{
	lip_ns_context_t* ns = lip_begin_ns(ctx, lip_string_ref(""));
	lip_declare_function(ns, lip_string_ref("nop"), nop);
	lip_declare_function(ns, lip_string_ref("identity"), identity);
	lip_declare_function(ns, lip_string_ref("print"), print);
	lip_end_ns(ctx, ns);
}
