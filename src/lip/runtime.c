#include "ex/runtime.h"
#include "temp_allocator.h"
#include "utils.h"

LIP_IMPLEMENT_DESTRUCTOR(lip_runtime)

lip_runtime_t*
lip_runtime_create(lip_allocator_t* allocator, lip_runtime_config_t* config)
{
	lip_runtime_t* runtime = lip_new(allocator, lip_runtime_t);
	lip_runtime_init(runtime, allocator, config);
	return runtime;
}

void
lip_runtime_init(
	lip_runtime_t* runtime,
	lip_allocator_t* allocator,
	lip_runtime_config_t* config
)
{
	runtime->allocator = allocator;
	runtime->temp_allocator = lip_temp_allocator_create(allocator, 1024);
	runtime->config = *config;
	lip_parser_init(&runtime->parser, allocator);
	lip_compiler_init(&runtime->compiler, allocator);
	runtime->last_error = NULL;
	runtime->default_vm = NULL;
}

void
lip_runtime_cleanup(lip_runtime_t* runtime)
{
	if(runtime->default_vm)
	{
		lip_vm_destroy(runtime->allocator, runtime->default_vm);\
	}

	lip_compiler_cleanup(&runtime->compiler);
	lip_parser_cleanup(&runtime->parser);
	lip_temp_allocator_destroy(runtime->temp_allocator);
}

lip_error_t*
lip_runtime_last_error(lip_runtime_t* runtime)
{
	return runtime->last_error;
}

bool
lip_runtime_exec(
	lip_runtime_t* runtime,
	lip_in_t* stream,
	lip_string_ref_t name,
	lip_vm_t* vm,
	lip_value_t* result
)
{
	lip_function_t* function = lip_runtime_compile(runtime, stream, name);
	if(function == NULL) { return false; }

	lip_closure_t* closure = lip_new(runtime->temp_allocator, lip_closure_t);
	closure->function.lip = function;
	closure->is_native = false;
	closure->env_len = 0;

	if(!vm)
	{
		vm = lip_runtime_default_vm(runtime);
	}

	lip_vm_reset(vm);
	lip_vm_push_closure(vm, closure);
	lip_exec_status_t status = lip_vm_call(vm, 0, result);
	lip_temp_allocator_reset(runtime->temp_allocator);
	lip_free(runtime->allocator, function);

	switch(status)
	{
		case LIP_EXEC_OK:
			return true;
		case LIP_EXEC_ERROR:
			return false;
	}

	return false;
}

lip_function_t*
lip_runtime_compile(
	lip_runtime_t* runtime,
	lip_in_t* stream,
	lip_string_ref_t name
)
{
	runtime->last_error = NULL;
	lip_compiler_begin(&runtime->compiler, name);
	lip_parser_reset(&runtime->parser, stream);

	while(true)
	{
		lip_sexp_t sexp;
		lip_stream_status_t stream_status =
			lip_parser_next_sexp(&runtime->parser, &sexp);

		switch(stream_status)
		{
			case LIP_STREAM_OK:
				{
					lip_error_t* compile_error =
						lip_compiler_add_sexp(&runtime->compiler, &sexp);
					if(compile_error != NULL)
					{
						runtime->last_error = compile_error;
						return NULL;
					}
				}
				break;
			case LIP_STREAM_ERROR:
				{
					lip_error_t* parse_error =
						lip_parser_last_error(&runtime->parser);
					if(parse_error != NULL)
					{
						return NULL;
					}
				}
				break;
			case LIP_STREAM_END:
				return lip_compiler_end(&runtime->compiler);
		}
	}
}

lip_vm_t*
lip_runtime_alloc_vm(lip_runtime_t* runtime, lip_vm_config_t* config)
{
	return lip_vm_create(
		runtime->allocator,
		config ? config : &runtime->config.vm_config
	);
}

void
lip_runtime_free_vm(lip_runtime_t* runtime, lip_vm_t* vm)
{
	lip_vm_destroy(runtime->allocator, vm);
}

lip_vm_t*
lip_runtime_default_vm(lip_runtime_t* runtime)
{
	if(runtime->default_vm)
	{
		return runtime->default_vm;
	}
	else
	{
		lip_vm_t* vm = lip_runtime_alloc_vm(runtime, NULL);
		runtime->default_vm = vm;
		return vm;
	}
}
