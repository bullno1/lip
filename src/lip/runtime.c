#include "runtime.h"
#include <stdarg.h>
#include "allocator.h"
#include "array.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "bundler.h"
#include "linker.h"
#include "vm.h"
#include "module.h"
#include "utils.h"

#define log_error(RUNTIME, FORMAT, ...) \
	lip_printf(\
		(RUNTIME)->config.error_fn, (RUNTIME)->config.error_ctx,\
		(FORMAT), __VA_ARGS__)

typedef struct lip_runtime_t {
	lip_runtime_config_t config;

	lip_lexer_t lexer;
	lip_parser_t parser;
	lip_compiler_t compiler;
	lip_bundler_t bundler;
	lip_linker_t linker;
	lip_vm_t vm;
	lip_array(lip_sexp_t) temp_sexps;
	void* vm_mem;
} lip_runtime_t;

lip_runtime_t* lip_runtime_new(lip_runtime_config_t* config)
{
	lip_allocator_t* allocator = config->allocator;
	lip_runtime_t* runtime = lip_new(allocator, lip_runtime_t);
	runtime->config = *config;
	lip_lexer_init(&runtime->lexer, allocator);
	lip_parser_init(&runtime->parser, &runtime->lexer, allocator);
	lip_compiler_init(&runtime->compiler, allocator);
	lip_bundler_init(&runtime->bundler, allocator);
	lip_linker_init(&runtime->linker, allocator);
	size_t mem_required = lip_vm_config(&runtime->vm, &config->vm_config);
	void* vm_mem = lip_malloc(allocator, mem_required);
	lip_vm_init(&runtime->vm, vm_mem);
	runtime->vm_mem = vm_mem;
	runtime->temp_sexps = lip_array_new(allocator);
	return runtime;
}

void lip_runtime_delete(lip_runtime_t* runtime)
{
	lip_array_foreach(lip_sexp_t, itr, runtime->temp_sexps)
	{
		lip_sexp_cleanup(itr);
	}
	lip_array_delete(runtime->temp_sexps);
	lip_allocator_t* allocator = runtime->config.allocator;
	lip_free(allocator, runtime->vm_mem);
	lip_array_foreach(lip_module_t*, itr, runtime->linker.modules)
	{
		lip_module_free(allocator, *itr);
	}
	lip_linker_cleanup(&runtime->linker);
	lip_bundler_cleanup(&runtime->bundler);
	lip_compiler_cleanup(&runtime->compiler);
	lip_lexer_cleanup(&runtime->lexer);
	lip_free(allocator, runtime);
}

lip_module_t* lip_runtime_load_filen(
	lip_runtime_t* runtime, const char* filename
)
{
	lip_module_t* module = lip_runtime_compile_filen(runtime, filename);

	if(module)
	{
		lip_runtime_load_module(runtime, module);
	}

	return module;
}

lip_module_t* lip_runtime_load_fileh(
	lip_runtime_t* runtime, const char* filename, FILE* file
)
{
	lip_module_t* module = lip_runtime_compile_fileh(runtime, filename, file);

	if(module)
	{
		lip_runtime_load_module(runtime, module);
	}

	return module;
}

lip_module_t* lip_runtime_load_stream(
	lip_runtime_t* runtime,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
)
{
	lip_module_t* module = lip_runtime_compile_stream(
		runtime, filename, read_fn, stream
	);

	if(module)
	{
		lip_runtime_load_module(runtime, module);
	}

	return module;
}

lip_module_t* lip_runtime_compile_filen(
	lip_runtime_t* runtime, const char* filename
)
{
	FILE* file = fopen(filename, "rb");
	if(file)
	{
		log_error(runtime, "Could not open '%s' for reading\n", filename);
		return NULL;
	}
	else
	{
		lip_module_t* module =
			lip_runtime_compile_fileh(runtime, filename, file);
		fclose(file);
		return module;
	}
}

lip_module_t* lip_runtime_compile_fileh(
	lip_runtime_t* runtime, const char* filename, FILE* file
)
{
	return lip_runtime_compile_stream(
		runtime,
		filename,
		lip_fread,
		file
	);
}

lip_module_t* lip_runtime_compile_stream(
	lip_runtime_t* runtime,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
)
{
	lip_array_foreach(lip_sexp_t, itr, runtime->temp_sexps)
	{
		lip_sexp_cleanup(itr);
	}
	lip_array_resize(runtime->temp_sexps, 0);

	lip_compiler_begin(&runtime->compiler, LIP_COMPILE_MODE_MODULE);

	lip_lexer_reset(&runtime->lexer, read_fn, stream);
	while(true)
	{
		lip_parse_status_t parse_status;
		lip_parse_result_t parse_result;

		parse_status = lip_parser_next_sexp(&runtime->parser, &parse_result);

		if(parse_status == LIP_PARSE_OK)
		{
			lip_array_push(runtime->temp_sexps, parse_result.sexp);

			lip_compile_error_t compile_error;
			bool compile_status = lip_compiler_add_sexp(
				&runtime->compiler, &parse_result.sexp, &compile_error
			);
			if(!compile_status)
			{
				log_error(
					runtime,
					"%s:%d:%d - %d:%d: %s\n",
					filename,
					compile_error.sexp->start.line,
					compile_error.sexp->start.column,
					compile_error.sexp->end.line,
					compile_error.sexp->end.column,
					compile_error.msg
				);
				return NULL;
			}
		}
		else if(parse_status == LIP_PARSE_EOS)
		{
			return lip_compiler_end(&runtime->compiler);
		}
		else
		{
			log_error(runtime, "%s: ", filename);
			lip_parser_print_status(
				runtime->config.error_fn, runtime->config.error_ctx,
				parse_status, &parse_result
			);
			lip_printf(
				runtime->config.error_fn, runtime->config.error_ctx, "\n"
			);
			return NULL;
		}
	}
}

void lip_runtime_load_module(lip_runtime_t* runtime, lip_module_t* module)
{
	lip_linker_add_module(&runtime->linker, module);
}

void lip_runtime_unload_module(lip_runtime_t* runtime, lip_module_t* module)
{
	lip_linker_remove_module(&runtime->linker, module);
}

void lip_runtime_free_module(lip_runtime_t* runtime, lip_module_t* module)
{
	lip_module_free(runtime->config.allocator, module);
}

void lip_runtime_exec_filen(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	void* context,
	const char* filename
)
{
	FILE* file = fopen(filename, "rb");
	if(file)
	{
		log_error(runtime, "Could not open '%s' for reading\n", filename);
	}
	else
	{
		lip_runtime_exec_fileh(runtime, handler, context, filename, file);
		fclose(file);
	}
}

void lip_runtime_exec_fileh(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	void* context,
	const char* filename,
	FILE* file
)
{
	lip_runtime_exec_stream(runtime, handler, context, filename, lip_fread, file);
}

void lip_runtime_exec_stream(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	void* context,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
)
{
	lip_lexer_reset(&runtime->lexer, read_fn, stream);
	lip_value_t error = { .type = LIP_VAL_NIL };
	while(true)
	{
		lip_parse_status_t parse_status;
		lip_parse_result_t parse_result;

		parse_status = lip_parser_next_sexp(&runtime->parser, &parse_result);

		if(parse_status == LIP_PARSE_OK)
		{
			lip_compile_error_t compile_error;
			lip_compiler_begin(&runtime->compiler, LIP_COMPILE_MODE_REPL);
			bool compile_status = lip_compiler_add_sexp(
				&runtime->compiler, &parse_result.sexp, &compile_error
			);
			if(!compile_status)
			{
				log_error(
					runtime,
					"%s:%d:%d - %d:%d: %s\n",
					filename,
					compile_error.sexp->start.line,
					compile_error.sexp->start.column,
					compile_error.sexp->end.line,
					compile_error.sexp->end.column,
					compile_error.msg
				);
				lip_sexp_cleanup(&parse_result.sexp);

				if(handler(context, LIP_EXEC_ERROR, &error))
				{
					continue;
				}
				else
				{
					return;
				}
			}

			lip_module_t* module = lip_compiler_end(&runtime->compiler);
			lip_sexp_cleanup(&parse_result.sexp);

			lip_runtime_load_module(runtime, module);
			lip_value_t result;
			lip_exec_status_t status = lip_runtime_exec_symbol(
				runtime, "main", &result
			);
			bool should_continue = handler(context, status, &result);
			lip_runtime_unload_module(runtime, module);
			lip_runtime_free_module(runtime, module);
			if(should_continue)
			{
				continue;
			}
			else
			{
				return;
			}
		}
		else if(parse_status == LIP_PARSE_EOS)
		{
			return;
		}
		else
		{
			log_error(runtime, "%s: ", filename);
			lip_parser_print_status(
				runtime->config.error_fn, runtime->config.error_ctx,
				parse_status, &parse_result
			);
			lip_printf(
				runtime->config.error_fn, runtime->config.error_ctx, "\n"
			);
			handler(context, LIP_EXEC_ERROR, &error);
			return;
		}
	}
}

lip_module_t* lip_runtime_register_native_module(
	lip_runtime_t* runtime,
	unsigned int num_entries,
	lip_native_module_entry_t* entries
)
{
	lip_module_t* module = lip_runtime_create_native_module(
		runtime, num_entries, entries
	);
	lip_runtime_load_module(runtime, module);
	return module;
}

lip_module_t* lip_runtime_create_native_module(
	lip_runtime_t* runtime,
	unsigned int num_entries,
	lip_native_module_entry_t* entries
)
{
	lip_bundler_begin(&runtime->bundler);
	lip_native_module_entry_t* itr;
	lip_native_module_entry_t* end = entries + num_entries;
	for(itr = entries; itr != end; ++itr)
	{
		lip_bundler_add_native_function(
			&runtime->bundler,
			lip_string_ref(itr->name),
			itr->function,
			itr->arity
		);
	}
	return lip_bundler_end(&runtime->bundler);
}

lip_exec_status_t lip_runtime_exec_symbol(
	lip_runtime_t* runtime,
	const char* symbol,
	lip_value_t* result
)
{
	lip_linker_link_modules(&runtime->linker);
	lip_value_t sym_val;
	lip_linker_find_symbol(&runtime->linker, lip_string_ref(symbol), &sym_val);
	lip_vm_push_value(&runtime->vm, &sym_val);
	lip_exec_status_t status = lip_vm_call(&runtime->vm, 0);
	*result = *lip_vm_pop(&runtime->vm);
	return status;
}

lip_vm_t* lip_runtime_get_vm(lip_runtime_t* runtime)
{
	return &runtime->vm;
}

lip_linker_t* lip_runtime_get_linker(lip_runtime_t* runtime)
{
	return &runtime->linker;
}
