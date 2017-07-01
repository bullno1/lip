#include "lip_internal.h"
#include <lip/io.h>
#include <lip/pp.h>

struct lip_prefix_stream_s
{
	lip_in_t vtable;
	lip_in_t* inner_stream;
	char* prefix;
	size_t prefix_len;
};

static const char LIP_BINARY_MAGIC[] = {'L', 'I', 'P', 0};

static size_t
lip_prefix_stream_read(void* buff, size_t size, lip_in_t* vtable)
{
	struct lip_prefix_stream_s* pstream =
		LIP_CONTAINER_OF(vtable, struct lip_prefix_stream_s, vtable);
	if(pstream->prefix_len > 0)
	{
		size_t num_bytes_to_read = LIP_MIN(pstream->prefix_len, size);
		memcpy(buff, pstream->prefix, num_bytes_to_read);
		pstream->prefix += num_bytes_to_read;
		pstream->prefix_len -= num_bytes_to_read;
		return num_bytes_to_read;
	}
	else
	{
		return lip_read(buff, size, pstream->inner_stream);
	}
}

static lip_in_t*
lip_make_prefix_stream(struct lip_prefix_stream_s* pstream)
{
	pstream->vtable.read = lip_prefix_stream_read;
	return &pstream->vtable;
}

static lip_function_t*
lip_load_bytecode(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
{
#define lip_checked_read(buff, size, input) \
	do { \
		if(lip_read(buff, size, input) != (size)) { \
			lip_fs_t* fs = ctx->runtime->cfg.fs; \
			lip_set_context_error( \
				ctx, "IO error", fs->last_error(fs), filename, LIP_LOC_NOWHERE \
			); \
			return NULL; \
		} \
	} while(0)

	uint8_t ptr_size;
	lip_checked_read(&ptr_size, sizeof(ptr_size), input);
	uint16_t bom;
	lip_checked_read(&bom, sizeof(bom), input);

	if(ptr_size != sizeof(void*) || bom != 1)
	{
		lip_set_context_error(
			ctx, "Format error",
			lip_string_ref("Incompatible bytecode"), filename, LIP_LOC_NOWHERE
		);
		return NULL;
	}

	lip_function_t header;
	lip_checked_read(&header, sizeof(header), input);
	if(header.size <= sizeof(header))
	{
		lip_set_context_error(
			ctx, "Format error",
			lip_string_ref("Malformed bytecode"), filename, LIP_LOC_NOWHERE
		);
		return NULL;
	}

	lip_function_t* function = lip_malloc(ctx->allocator, header.size);
	if(function == NULL)
	{
		lip_set_context_error(
			ctx, "Loading error",
			lip_string_ref("Out of memory"), filename, LIP_LOC_NOWHERE
		);
		return NULL;
	}

	memcpy(function, &header, sizeof(header));
	size_t body_size = header.size - sizeof(header);
	if(lip_read((char*)function + sizeof(header), body_size, input) != body_size)
	{
		lip_free(ctx->allocator, function);
		lip_set_context_error(
			ctx, "IO error",
			ctx->runtime->cfg.fs->last_error(ctx->runtime->cfg.fs),
			filename, LIP_LOC_NOWHERE
		);
		return NULL;
	}

	return function;
}

static lip_function_t*
lip_load_source(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
{
	lip_arena_allocator_reset(ctx->temp_pool);
	lip_parser_reset(&ctx->parser, input);
	lip_compiler_begin(&ctx->compiler, filename);

	lip_pp_t pp = {
		.allocator = ctx->temp_pool
	};

	for(;;)
	{
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
							filename,
							pp_result.value.error.location
						);

						return NULL;
					}

					lip_ast_result_t ast_result = lip_translate_sexp(
						ctx->temp_pool, pp_result.value.result
					);
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
				return lip_compiler_end(&ctx->compiler, ctx->allocator);
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

static lip_function_t*
lip_load_function(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
{
	char magic[sizeof(LIP_BINARY_MAGIC)];
	size_t bytes_read = lip_read(magic, sizeof(magic), input);
	bool is_binary = true
		&& bytes_read == sizeof(magic)
		&& memcmp(magic, LIP_BINARY_MAGIC, sizeof(LIP_BINARY_MAGIC)) == 0;

	if(is_binary)
	{
		return lip_load_bytecode(ctx, filename, input);
	}
	else
	{
		struct lip_prefix_stream_s pstream = {
			.inner_stream = input,
			.prefix = magic,
			.prefix_len = bytes_read
		};
		return lip_load_source(ctx, filename, lip_make_prefix_stream(&pstream));
	}
}

static bool
lip_do_dump_script(
	lip_context_t* ctx,
	lip_script_t* script,
	lip_string_ref_t filename,
	lip_out_t* output
)
{
#define lip_checked_write(buff, size, output) \
	do { \
		if(lip_write(buff, size, output) != (size)) { \
			lip_fs_t* fs = ctx->runtime->cfg.fs; \
			lip_set_context_error( \
				ctx, "IO error", fs->last_error(fs), filename, LIP_LOC_NOWHERE \
			); \
			return false; \
		} \
	} while(0)

	lip_closure_t* closure = script->closure;

	lip_checked_write(LIP_BINARY_MAGIC, sizeof(LIP_BINARY_MAGIC), output);
	uint8_t ptr_size = sizeof(void*);
	lip_checked_write(&ptr_size, sizeof(ptr_size), output);
	uint16_t bom = 1;
	lip_checked_write(&bom, sizeof(bom), output);

	lip_checked_write(closure->function.lip, closure->function.lip->size, output);

	return true;
}

lip_script_t*
lip_load_script(
	lip_context_t* ctx,
	lip_string_ref_t filename,
	lip_in_t* input
)
{
	bool own_input = input == NULL;
	if(own_input)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;
		input = fs->begin_read(fs, filename);
		if(input == NULL)
		{
			lip_set_context_error(
				ctx, "IO error", fs->last_error(fs), filename, LIP_LOC_NOWHERE
			);
			return NULL;
		}
	}

	lip_function_t* fn = lip_load_function(ctx, filename, input);

	lip_script_t* script = NULL;
	lip_ctx_begin_load(ctx);
	if(fn)
	{
		lip_closure_t* closure = lip_new(ctx->allocator, lip_closure_t);
		*closure = (lip_closure_t){
			.function = { .lip = fn },
			.is_native = false,
			.env_len = 0,
		};

		script = lip_new(ctx->allocator, lip_script_t);
		*script = (lip_script_t){ .closure = closure, .linked = false };
	}

	lip_ctx_end_load(ctx);

	if(own_input)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;
		fs->end_read(fs, input);
	}

	return script;
}

LIP_API bool
lip_dump_script(
	lip_context_t* ctx,
	lip_script_t* script,
	lip_string_ref_t filename,
	lip_out_t* output
)
{
	bool own_output = output == NULL;
	if(own_output)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;

		output = fs->begin_write(fs, filename);
		if(output == NULL)
		{
			lip_set_context_error(
				ctx, "IO error", fs->last_error(fs), filename, LIP_LOC_NOWHERE
			);
			return false;
		}
	}

	bool result = lip_do_dump_script(ctx, script, filename, output);

	if(own_output)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;
		fs->end_write(fs, output);
	}

	return result;
}

void
lip_unload_script(lip_context_t* ctx, lip_script_t* script)
{
	lip_closure_t* closure = script->closure;
	khiter_t itr = kh_get(lip_ptr_set, ctx->new_script_functions, closure->function.lip);
	if(itr != kh_end(ctx->new_script_functions))
	{
		kh_del(lip_ptr_set, ctx->new_script_functions, itr);
	}

	lip_free(ctx->allocator, closure->function.lip);
	lip_free(ctx->allocator, closure);
	lip_free(ctx->allocator, script);
}

lip_exec_status_t
(lip_exec_script)(lip_vm_t* vm, lip_script_t* script, lip_value_t* result)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);

	if(!script->linked)
	{
		lip_function_t* fn = script->closure->function.lip;
		lip_ctx_begin_load(rt->ctx);
		bool linked = lip_link_function(rt->ctx, fn);
		lip_ctx_end_load(rt->ctx);
		script->linked = linked;
		if(!linked) { return LIP_EXEC_ERROR; }
	}

	lip_exec_status_t status = (lip_call)(
		vm,
		result,
		(lip_value_t){
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = script->closure }
		},
		0
	);
	rt->ctx->last_result = *result;
	rt->ctx->last_vm = vm;
	return status;
}
