#include "lip.h"
#include "vendor/khash.h"
#include "ex/vm.h"
#include "ex/compiler.h"
#include "ex/parser.h"
#include "array.h"
#include "ast.h"
#include "io.h"

#define CHECK(cond, msg) \
	do { \
		if(!(cond)) { \
			ctx->error.message = lip_string_ref(msg); \
			return false; \
		} \
	} while(0)

KHASH_DECLARE(lip_ptr_set, void*, char)

struct lip_runtime_s
{
	lip_allocator_t* allocator;
	khash_t(lip_ptr_set)* contexts;
};

struct lip_context_s
{
	lip_runtime_t* runtime;
	lip_allocator_t* allocator;
	lip_allocator_t* arena_allocator;
	lip_array(lip_error_record_t) error_records;
	lip_context_error_t error;
	lip_parser_t parser;
	lip_compiler_t compiler;
	khash_t(lip_ptr_set)* scripts;
	khash_t(lip_ptr_set)* vms;
};

lip_runtime_t*
lip_create_runtime(lip_allocator_t* allocator)
{
	lip_runtime_t* runtime = lip_new(allocator, lip_runtime_t);
	runtime->allocator = allocator;
	runtime->contexts = kh_init(lip_ptr_set, allocator);
	return runtime;
}

static void
lip_do_unload_script(lip_context_t* ctx, lip_script_t* script)
{
	lip_closure_t* closure = (lip_closure_t*)script;
	lip_free(ctx->allocator, closure->function.lip);
	lip_free(ctx->allocator, closure);
}

static void
lip_do_destroy_vm(lip_context_t* ctx, lip_vm_t* vm)
{
	lip_vm_destroy(ctx->allocator, vm);
}

static void
lip_do_destroy_context(lip_runtime_t* runtime, lip_context_t* ctx)
{
	(void)runtime;

	for(
		khint_t i = kh_begin(ctx->vms);
		i != kh_end(ctx->vms);
		++i
	)
	{
		if(!kh_exist(ctx->vms, i)) { continue; }

		lip_do_destroy_vm(ctx, kh_key(ctx->vms, i));
	}

	for(
		khint_t i = kh_begin(ctx->scripts);
		i != kh_end(ctx->scripts);
		++i
	)
	{
		if(!kh_exist(ctx->scripts, i)) { continue; }

		lip_do_unload_script(ctx, kh_key(ctx->scripts, i));
	}

	kh_destroy(lip_ptr_set, ctx->vms);
	kh_destroy(lip_ptr_set, ctx->scripts);
	lip_compiler_cleanup(&ctx->compiler);
	lip_parser_cleanup(&ctx->parser);
	lip_array_destroy(ctx->error_records);
	lip_arena_allocator_destroy(ctx->arena_allocator);
	lip_free(ctx->allocator, ctx);
}

void
lip_destroy_runtime(lip_runtime_t* runtime)
{
	for(
		khint_t i = kh_begin(runtime->contexts);
		i != kh_end(runtime->contexts);
		++i
	)
	{
		if(!kh_exist(runtime->contexts, i)) { continue; }

		lip_do_destroy_context(runtime, kh_key(runtime->contexts, i));
	}

	kh_destroy(lip_ptr_set, runtime->contexts);
	lip_free(runtime->allocator, runtime);
}

lip_context_t*
lip_create_context(lip_runtime_t* runtime, lip_allocator_t* allocator)
{
	if(allocator == NULL) { allocator = runtime->allocator; }

	lip_context_t* ctx = lip_new(allocator, lip_context_t);
	ctx->allocator = allocator;
	ctx->runtime = runtime;
	ctx->arena_allocator = lip_arena_allocator_create(allocator, 2048);
	ctx->error_records = lip_array_create(allocator, lip_error_record_t, 1);
	lip_parser_init(&ctx->parser, allocator);
	lip_compiler_init(&ctx->compiler, allocator);
	ctx->scripts = kh_init(lip_ptr_set, allocator);
	ctx->vms = kh_init(lip_ptr_set, allocator);

	int tmp;
	kh_put(lip_ptr_set, runtime->contexts, ctx, &tmp);

	return ctx;
}


void
lip_destroy_context(lip_runtime_t* runtime, lip_context_t* ctx)
{
	khiter_t itr = kh_get(lip_ptr_set, runtime->contexts, ctx);
	if(itr != kh_end(runtime->contexts))
	{
		kh_del(lip_ptr_set, runtime->contexts, itr);
		lip_do_destroy_context(runtime, ctx);
	}
}

const lip_context_error_t*
lip_get_error(lip_context_t* ctx)
{
	return &ctx->error;
}

lip_vm_t*
lip_create_vm(lip_context_t* ctx, lip_vm_config_t* config)
{
	lip_vm_t* vm = lip_vm_create(ctx->allocator, config);

	int tmp;
	kh_put(lip_ptr_set, ctx->vms, vm, &tmp);

	return vm;
}

void
lip_destroy_vm(lip_context_t* ctx, lip_vm_t* vm)
{
	khiter_t itr = kh_get(lip_ptr_set, ctx->vms, vm);
	if(itr != kh_end(ctx->vms))
	{
		kh_del(lip_ptr_set, ctx->vms, itr);
		lip_do_destroy_vm(ctx, vm);
	}
}

static lip_string_ref_t
lip_format_parse_error(const lip_error_t* error)
{
	switch((lip_parse_error_t)error->code)
	{
		case LIP_PARSE_LEX_ERROR:
			{
				const lip_error_t* lex_error = error->extra;
				switch((lip_lex_error_t)lex_error->code)
				{
					case LIP_LEX_BAD_NUMBER:
						return lip_string_ref("Malformed number");
					case LIP_LEX_BAD_STRING:
						return lip_string_ref("Malformed string");
				}
			}
		case LIP_PARSE_UNEXPECTED_TOKEN:
			return lip_string_ref("Unexpected token");
		case LIP_PARSE_UNTERMINATED_LIST:
			return lip_string_ref("Unterminated list");
	}

	return lip_string_ref("Unknown error");
}

static void
lip_set_compile_error(
	lip_context_t* ctx,
	lip_string_ref_t message,
	lip_string_ref_t filename,
	lip_loc_range_t location
)
{
	lip_array_clear(ctx->error_records);
	lip_error_record_t* record = lip_array_alloc(ctx->error_records);
	record->message = filename;
	record->location = location;
	ctx->error.message = message;
	ctx->error.num_records = 1;
	ctx->error.records = ctx->error_records;
}

lip_script_t*
lip_load_script(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
{
	lip_arena_allocator_reset(ctx->arena_allocator);
	lip_parser_reset(&ctx->parser, input);
	lip_compiler_begin(&ctx->compiler, filename);

	for(;;)
	{
		lip_sexp_t sexp;
		switch(lip_parser_next_sexp(&ctx->parser, &sexp))
		{
			case LIP_STREAM_OK:
				{
					lip_ast_result_t ast_result =
						lip_translate_sexp(ctx->arena_allocator, &sexp);
					if(!ast_result.success)
					{
						lip_set_compile_error(
							ctx,
							lip_string_ref(ast_result.value.error.extra),
							filename,
							ast_result.value.error.location
						);

						return NULL;
					}

					lip_compiler_add_ast(&ctx->compiler, ast_result.value.result);
				}
				break;
			case LIP_STREAM_END:
				{
					lip_function_t* fn = lip_compiler_end(&ctx->compiler, ctx->allocator);
					lip_closure_t* closure = lip_new(ctx->allocator, lip_closure_t);
					closure->function.lip = fn;
					closure->is_native = false;
					closure->env_len = 0;
					closure->no_gc = true;

					int tmp;
					kh_put(lip_ptr_set, ctx->scripts, closure, &tmp);

					return (void*)closure;
				}
				break;
			case LIP_STREAM_ERROR:
				{
					const lip_error_t* error = lip_parser_last_error(&ctx->parser);
					lip_set_compile_error(
						ctx,
						lip_format_parse_error(error),
						filename,
						error->location
					);

					return NULL;
				}
				break;
		}
	}

	return NULL;
}

void
lip_unload_script(lip_context_t* ctx, lip_script_t* script)
{
	khiter_t itr = kh_get(lip_ptr_set, ctx->scripts, script);
	if(itr != kh_end(ctx->scripts))
	{
		kh_del(lip_ptr_set, ctx->scripts, itr);
		lip_do_unload_script(ctx, script);
	}
}

lip_exec_status_t
lip_exec_script(lip_vm_t* vm, lip_script_t* script, lip_value_t* result)
{
	lip_vm_push_value(vm, (lip_value_t){
		.type = LIP_VAL_FUNCTION,
		.data = { .reference = script }
	});
	return lip_vm_call(vm, 0, result);
}
