#include "lip_internal.h"
#include <lip/io.h>
#include <lip/pp.h>

typedef struct lip_repl_stream_s lip_repl_stream_t;

struct lip_repl_stream_s
{
	lip_in_t vtable;
	lip_repl_handler_t* repl_handler;
};

static size_t
lip_repl_read(void* buff, size_t size, lip_in_t* vtable)
{
	lip_repl_stream_t* stream = LIP_CONTAINER_OF(vtable, lip_repl_stream_t, vtable);
	return stream->repl_handler->read(stream->repl_handler, buff, size);
}

void
lip_repl(
	lip_vm_t* vm, lip_string_ref_t source_name, lip_repl_handler_t* repl_handler
)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	lip_context_t* ctx = rt->ctx;

	struct lip_repl_stream_s input = {
		.repl_handler = repl_handler,
		.vtable = { .read = lip_repl_read }
	};

	lip_pp_t pp = {
		.allocator = ctx->temp_pool
	};

	for(;;)
	{
		lip_arena_allocator_reset(ctx->temp_pool);
		lip_parser_reset(&ctx->parser, &input.vtable);

		lip_sexp_t sexp;
		switch(lip_parser_next_sexp(&ctx->parser, &sexp))
		{
			case LIP_STREAM_OK:
				{
					lip_pp_result_t pp_result = lip_preprocess(&pp, &sexp);
					if(!pp_result.success)
					{
						lip_set_compile_error(
							ctx,
							lip_string_ref(pp_result.value.error.extra),
							source_name,
							pp_result.value.error.location
						);
						repl_handler->print(
							repl_handler,
							LIP_EXEC_ERROR,
							(lip_value_t) { .type = LIP_VAL_NIL }
						);
						continue;
					}

					lip_ast_result_t ast_result = lip_translate_sexp(
						ctx->temp_pool, pp_result.value.result
					);

					if(!ast_result.success)
					{
						lip_set_compile_error(
							ctx,
							lip_string_ref(ast_result.value.error.extra),
							source_name,
							ast_result.value.error.location
						);
						repl_handler->print(
							repl_handler,
							LIP_EXEC_ERROR,
							(lip_value_t) { .type = LIP_VAL_NIL }
						);
						continue;
					}

					lip_compiler_begin(&ctx->compiler, source_name);
					lip_compiler_add_ast(&ctx->compiler, ast_result.value.result);
					lip_function_t* fn = lip_compiler_end(&ctx->compiler, ctx->temp_pool);
					lip_ctx_begin_load(ctx);
					bool link_result = lip_link_function(ctx, fn);
					if(link_result)
					{
						int ret;
						kh_put(lip_ptr_set, ctx->new_script_functions, fn, &ret);
					}
					lip_ctx_end_load(ctx);

					if(!link_result)
					{
						repl_handler->print(
							repl_handler,
							LIP_EXEC_ERROR,
							(lip_value_t) { .type = LIP_VAL_NIL }
						);
						continue;
					}

					lip_closure_t* closure = lip_new(ctx->temp_pool, lip_closure_t);
					*closure = (lip_closure_t){
						.function = { .lip = fn },
						.is_native = false,
						.env_len = 0,
					};
					lip_reset_vm(vm);
					lip_value_t result;
					lip_exec_status_t status = lip_call(
						vm,
						&result,
						(lip_value_t){
							.type = LIP_VAL_FUNCTION,
							.data = { .reference = closure }
						},
						0
					);
					ctx->last_result = result;
					ctx->last_vm = vm;
					repl_handler->print(repl_handler, status, result);
				}
				break;
			case LIP_STREAM_ERROR:
				{
					const lip_error_t* error = lip_parser_last_error(&ctx->parser);
					lip_set_compile_error(
						ctx,
						lip_format_parse_error(error),
						source_name,
						error->location
					);
					repl_handler->print(
						repl_handler,
						LIP_EXEC_ERROR,
						(lip_value_t) { .type = LIP_VAL_NIL }
					);
				}
				break;
			case LIP_STREAM_END:
				return;
		}
	}
}
