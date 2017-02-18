#include "lip_internal.h"
#include <stdio.h>
#include <lip/array.h>
#include <lip/memory.h>
#include <lip/pp.h>
#include <lip/ast.h>
#include <lip/io.h>
#include <lip/vm.h>
#include <lip/print.h>

#define lip_assert(ctx, cond) \
	do { \
		if(!(cond)) { \
			lip_panic( \
				ctx, \
				__FILE__ ":" lip_stringify(__LINE__) ": Assertion '" #cond "' failed." \
			); \
		} \
	} while(0)
#define lip_stringify(x) lip_stringify2(x)
#define lip_stringify2(x) #x

typedef struct lip_repl_stream_s lip_repl_stream_t;

struct lip_repl_stream_s
{
	lip_in_t vtable;
	lip_repl_handler_t* repl_handler;
};

void
lip_reset_runtime_config(lip_runtime_config_t* cfg)
{
	*cfg = (lip_runtime_config_t){
		.allocator = lip_default_allocator,
		.default_vm_config = {
			.os_len = 256,
			.cs_len = 256,
			.env_len = 256
		}
	};
}

lip_runtime_t*
lip_create_runtime(const lip_runtime_config_t* cfg)
{
	lip_runtime_t* runtime = lip_new(cfg->allocator, lip_runtime_t);

	bool own_fs = cfg->fs == NULL;
	lip_fs_t* fs = own_fs ? lip_create_native_fs(cfg->allocator) : cfg->fs;

	*runtime = (lip_runtime_t){
		.cfg = *cfg,
		.own_fs = own_fs,
		.contexts = kh_init(lip_ptr_set, cfg->allocator),
		.symtab = kh_init(lip_symtab, cfg->allocator),
	};
	runtime->cfg.fs = fs;

	lip_rwlock_init(&runtime->rt_lock);
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
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	if(rt->userdata) { kh_destroy(lip_userdata, rt->userdata); }
	lip_arena_allocator_destroy(rt->allocator);
	lip_free(ctx->allocator, vm);
}

static void
lip_do_destroy_context(lip_context_t* ctx)
{
	kh_foreach(itr, ctx->vms)
	{
		lip_do_destroy_vm(ctx, kh_key(ctx->vms, itr));
	}

	kh_foreach(itr, ctx->scripts)
	{
		lip_do_unload_script(ctx, kh_key(ctx->scripts, itr));
	}

	lip_array_destroy(ctx->string_buff);
	kh_destroy(lip_ptr_set, ctx->vms);
	kh_destroy(lip_ptr_set, ctx->scripts);
	lip_compiler_cleanup(&ctx->compiler);
	lip_parser_cleanup(&ctx->parser);
	lip_array_destroy(ctx->error_records);
	lip_arena_allocator_destroy(ctx->arena_allocator);
	if(ctx->userdata) { kh_destroy(lip_userdata, ctx->userdata); }
	lip_free(ctx->allocator, ctx);
}

static void
lip_purge_ns(lip_runtime_t* runtime, khash_t(lip_ns)* ns)
{
	kh_foreach(i, ns)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(ns, i).ptr);
		lip_free(runtime->cfg.allocator, kh_val(ns, i).value);
	}
	kh_clear(lip_ns, ns);
}

void
lip_destroy_runtime(lip_runtime_t* runtime)
{
	kh_foreach(itr, runtime->contexts)
	{
		lip_do_destroy_context(kh_key(runtime->contexts, itr));
	}

	kh_foreach(itr, runtime->symtab)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(runtime->symtab, itr).ptr);
		khash_t(lip_ns)* ns = kh_val(runtime->symtab, itr);
		lip_purge_ns(runtime, ns);
		kh_destroy(lip_ns, ns);
	}

	lip_rwlock_destroy(&runtime->rt_lock);
	kh_destroy(lip_symtab, runtime->symtab);
	kh_destroy(lip_ptr_set, runtime->contexts);
	if(runtime->userdata) { kh_destroy(lip_userdata, runtime->userdata); }
	if(runtime->own_fs) { lip_free(runtime->cfg.allocator, runtime->cfg.fs); }
	lip_free(runtime->cfg.allocator, runtime);
}

static void
lip_panic(lip_context_t* ctx, const char* msg)
{
	ctx->panic_handler(ctx, msg);
}

static void
lip_default_panic_handler(lip_context_t* ctx, const char* msg)
{
	(void)ctx;
	fprintf(stderr, "%s\n", msg);
	abort();
}

lip_context_t*
lip_create_context(lip_runtime_t* runtime, lip_allocator_t* allocator)
{
	if(allocator == NULL) { allocator = runtime->cfg.allocator; }

	lip_context_t* ctx = lip_new(allocator, lip_context_t);
	*ctx = (lip_context_t) {
		.allocator = allocator,
		.runtime = runtime,
		.arena_allocator = lip_arena_allocator_create(allocator, 2048),
		.error_records = lip_array_create(allocator, lip_error_record_t, 1),
		.scripts = kh_init(lip_ptr_set, allocator),
		.vms = kh_init(lip_ptr_set, allocator),
		.string_buff = lip_array_create(allocator, char, 512),
		.panic_handler = lip_default_panic_handler
	};
	lip_parser_init(&ctx->parser, allocator);
	lip_compiler_init(&ctx->compiler, allocator);

	int tmp;
	kh_put(lip_ptr_set, runtime->contexts, ctx, &tmp);

	return ctx;
}

lip_panic_fn_t
lip_set_panic_handler(lip_context_t* ctx, lip_panic_fn_t panic_handler)
{
	lip_panic_fn_t old_handler = ctx->panic_handler;
	ctx->panic_handler = panic_handler;
	return old_handler;
}

void
lip_destroy_context(lip_context_t* ctx)
{
	khiter_t itr = kh_get(lip_ptr_set, ctx->runtime->contexts, ctx);
	if(itr != kh_end(ctx->runtime->contexts))
	{
		kh_del(lip_ptr_set, ctx->runtime->contexts, itr);
		lip_do_destroy_context(ctx);
	}
}

const lip_context_error_t*
lip_get_error(lip_context_t* ctx)
{
	return &ctx->error;
}

static lip_string_ref_t
lip_function_name(lip_stack_frame_t* fp)
{
	if(fp->closure == NULL)
	{
		return fp->native_function
			? lip_string_ref(fp->native_function)
			: lip_string_ref("?");
	}
	else if(fp->closure->debug_name)
	{
		return (lip_string_ref_t) {
			.ptr = fp->closure->debug_name->ptr,
			.length = fp->closure->debug_name->length
		};
	}
	else if(fp->native_function)
	{
		return lip_string_ref(fp->native_function);
	}
	else
	{
		return lip_string_ref("?");
	}
}

const lip_context_error_t*
lip_traceback(lip_context_t* ctx, lip_vm_t* vm, lip_value_t msg)
{
	if(msg.type == LIP_VAL_STRING)
	{
		lip_string_t* str = msg.data.reference;
		ctx->error.message = (lip_string_ref_t){
			.ptr = str->ptr,
			.length = str->length
		};
	}
	else
	{
		lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
		lip_array_clear(rt->ctx->string_buff);
		struct lip_osstream_s osstream;
		lip_out_t* output = lip_make_osstream(&rt->ctx->string_buff, &osstream);
		lip_print_value(1, 0, output, msg);
		size_t msg_len = lip_array_len(rt->ctx->string_buff) - 1; // exclude new line
		ctx->error.message = (lip_string_ref_t){
			.ptr = rt->ctx->string_buff,
			.length = msg_len
		};
		rt->ctx->string_buff[msg_len] = '\0';
	}

	lip_array_clear(ctx->error_records);
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(&vm->config, &os_block, &env_block, &cs_block);
	lip_stack_frame_t* fp_min = lip_locate_memblock(vm->mem, &cs_block);
	for(lip_stack_frame_t* fp = vm->fp; fp >= fp_min; --fp)
	{
		if(lip_stack_frame_is_native(fp))
		{
			const char* filename = fp->native_filename ? fp->native_filename : "<native>";
			lip_loc_range_t location;
			if(fp->native_line)
			{
				location = (lip_loc_range_t){
					.start = { .line = fp->native_line },
					.end = { .line = fp->native_line }
				};
			}
			else
			{
				location = LIP_LOC_NOWHERE;
			}

			*lip_array_alloc(ctx->error_records) = (lip_error_record_t){
				.filename = lip_string_ref(filename),
				.location = location,
				.message = lip_function_name(fp)
			};
		}
		else
		{
			lip_function_layout_t function_layout;
			lip_function_layout(fp->closure->function.lip, &function_layout);
			lip_loc_range_t location =
				function_layout.locations[LIP_MAX(0, fp->pc - function_layout.instructions)];
			*lip_array_alloc(ctx->error_records) = (lip_error_record_t){
				.filename = (lip_string_ref_t){
					.ptr = function_layout.source_name->ptr,
					.length = function_layout.source_name->length
				},
				.location = location,
				.message = lip_function_name(fp)
			};
		}
	}
	ctx->error.num_records = lip_array_len(ctx->error_records);
	ctx->error.records = ctx->error_records;

	return &ctx->error;
}

lip_ns_context_t*
lip_begin_ns(lip_context_t* ctx, lip_string_ref_t name)
{
	lip_ns_context_t* ns_context = lip_new(ctx->allocator, lip_ns_context_t);
	*ns_context = (lip_ns_context_t){
		.allocator = lip_arena_allocator_create(ctx->allocator, 20480),
		.content = kh_init(lip_ns, ctx->allocator),
		.name = name
	};
	return ns_context;
}

static void
lip_commit_ns_locked(lip_context_t* ctx, lip_ns_context_t* ns_ctx)
{
	lip_runtime_t* runtime = ctx->runtime;
	khash_t(lip_symtab)* symtab = runtime->symtab;
	khiter_t itr = kh_get(lip_symtab, symtab, ns_ctx->name);

	khash_t(lip_ns)* ns;
	if(itr != kh_end(symtab))
	{
		ns = kh_val(symtab, itr);
		lip_purge_ns(runtime, ns);
	}
	else
	{
		void* str_buff = lip_malloc(runtime->cfg.allocator, ns_ctx->name.length);
		memcpy(str_buff, ns_ctx->name.ptr, ns_ctx->name.length);
		lip_string_ref_t ns_name = {.ptr = str_buff, .length = ns_ctx->name.length};
		ns = kh_init(lip_ns, runtime->cfg.allocator);

		int ret;
		khiter_t itr = kh_put(lip_symtab, symtab, ns_name, &ret);
		kh_val(symtab, itr) = ns;
	}

	kh_foreach(i, ns_ctx->content)
	{
		lip_string_ref_t symbol_name = kh_key(ns_ctx->content, i);
		lip_symbol_t symbol_value = kh_val(ns_ctx->content, i);

		void* str_buff = lip_malloc(runtime->cfg.allocator, symbol_name.length);
		memcpy(str_buff, symbol_name.ptr, symbol_name.length);
		symbol_name.ptr = str_buff;

		size_t closure_size =
			sizeof(lip_closure_t) +
			sizeof(lip_value_t) * symbol_value.value->env_len;
		lip_closure_t* closure = lip_malloc(runtime->cfg.allocator, closure_size);
		memcpy(closure, symbol_value.value, closure_size);
		symbol_value.value = closure;

		int ret;
		khiter_t itr = kh_put(lip_ns, ns, symbol_name, &ret);
		kh_val(ns, itr) = symbol_value;
	}
}

void
lip_end_ns(lip_context_t* ctx, lip_ns_context_t* ns)
{
	lip_assert(ctx, lip_rwlock_begin_write(&ctx->runtime->rt_lock));
	lip_commit_ns_locked(ctx, ns);
	lip_rwlock_end_write(&ctx->runtime->rt_lock);

	lip_discard_ns(ctx, ns);
}

void
lip_discard_ns(lip_context_t* ctx, lip_ns_context_t* ns)
{
	kh_destroy(lip_ns, ns->content);
	lip_arena_allocator_destroy(ns->allocator);
	lip_free(ctx->allocator, ns);
}

void
lip_declare_function(
	lip_ns_context_t* ns, lip_string_ref_t name, lip_native_fn_t fn
)
{
	int ret;
	khiter_t itr  = kh_put(lip_ns, ns->content, name, &ret);

	lip_closure_t* closure = lip_new(ns->allocator, lip_closure_t);
	*closure = (lip_closure_t){
		.is_native = true,
		.env_len = 0,
		.function = { .native = fn }
	};

	kh_val(ns->content, itr).value = closure;
}

static bool
lip_rt_resolve_import(
	lip_runtime_interface_t* vtable,
	lip_string_t* symbol_name,
	lip_value_t* result
)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vtable, lip_runtime_link_t, vtable);
	lip_string_ref_t symbol_name_ref = {
		.ptr = symbol_name->ptr,
		.length = symbol_name->length
	};
	return lip_lookup_symbol(rt->ctx, symbol_name_ref, result);
}

static void*
lip_rt_malloc(lip_runtime_interface_t* vtable, lip_value_type_t type, size_t size)
{
	(void)type;
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vtable, lip_runtime_link_t, vtable);
	return lip_malloc(rt->allocator, size);
}

static const char*
lip_rt_format(lip_runtime_interface_t* vtable, const char* fmt, va_list args)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vtable, lip_runtime_link_t, vtable);
	lip_array_clear(rt->ctx->string_buff);
	struct lip_osstream_s osstream;
	lip_out_t* output = lip_make_osstream(&rt->ctx->string_buff, &osstream);
	lip_vprintf(output, fmt, args);
	lip_array_push(rt->ctx->string_buff, '\0');
	return rt->ctx->string_buff;
}

lip_vm_t*
lip_create_vm(lip_context_t* ctx, const lip_vm_config_t* config)
{
	if(config == NULL) { config = &ctx->runtime->cfg.default_vm_config; }
	lip_memblock_info_t vm_block = {
		.element_size = sizeof(lip_vm_t),
		.num_elements = 1,
		.alignment = LIP_ALIGN_OF(lip_vm_t)
	};

	lip_memblock_info_t vm_mem_block = {
		.element_size = lip_vm_memory_required(config),
		.num_elements = 1,
		.alignment = LIP_MAX_ALIGNMENT
	};

	lip_memblock_info_t rt_link_block = {
		.element_size = sizeof(lip_runtime_link_t),
		.num_elements = 1,
		.alignment = LIP_ALIGN_OF(lip_runtime_link_t)
	};

	lip_memblock_info_t* vm_layout[] = { &vm_block, &vm_mem_block, &rt_link_block };
	lip_memblock_info_t block_info =
		lip_align_memblocks(LIP_STATIC_ARRAY_LEN(vm_layout), vm_layout);

	void* mem = lip_malloc(ctx->allocator, block_info.num_elements);
	lip_vm_t* vm = lip_locate_memblock(mem, &vm_block);
	void* vm_mem = lip_locate_memblock(mem, &vm_mem_block);
	lip_runtime_link_t* rt = lip_locate_memblock(mem, &rt_link_block);
	*rt = (lip_runtime_link_t) {
		.allocator = lip_arena_allocator_create(ctx->allocator, 1024),
		.ctx = ctx,
		.vtable = {
			.resolve_import = lip_rt_resolve_import,
			.malloc = lip_rt_malloc,
			.format = lip_rt_format
		}
	};
	lip_vm_init(vm, config, &rt->vtable, vm_mem);

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
lip_set_context_error(
	lip_context_t* ctx,
	const char* error_type,
	lip_string_ref_t message,
	lip_string_ref_t filename,
	lip_loc_range_t location
)
{
	lip_array_clear(ctx->error_records);
	lip_error_record_t* record = lip_array_alloc(ctx->error_records);
	record->filename = filename;
	record->location = location;
	record->message = message;
	ctx->error.message = lip_string_ref(error_type);
	ctx->error.num_records = 1;
	ctx->error.records = ctx->error_records;
}

static void
lip_set_compile_error(
	lip_context_t* ctx,
	lip_string_ref_t message,
	lip_string_ref_t filename,
	lip_loc_range_t location
)
{
	lip_set_context_error(ctx, "Syntax error", message, filename, location);
}

static bool
lip_lookup_symbol_locked(
	lip_context_t* ctx,
	lip_string_ref_t namespace,
	lip_string_ref_t symbol,
	lip_value_t* result
)
{
	khash_t(lip_symtab)* symtab = ctx->runtime->symtab;
	khiter_t itr = kh_get(lip_symtab, symtab, namespace);
	if(itr == kh_end(symtab)) { return false; }

	khash_t(lip_ns)* ns = kh_val(symtab, itr);
	itr = kh_get(lip_ns, ns, symbol);
	if(itr == kh_end(ns)) { return false; }

	*result = (lip_value_t){
		.type = LIP_VAL_FUNCTION,
		.data = { .reference = kh_val(ns, itr).value }
	};
	return true;
}

bool
lip_lookup_symbol(lip_context_t* ctx, lip_string_ref_t symbol_name, lip_value_t* result)
{
	lip_string_ref_t namespace;
	lip_string_ref_t symbol;

	if(symbol_name.length == 1 && symbol_name.ptr[0] == '/')
	{
		namespace = lip_string_ref("");
		symbol = symbol_name;
	}
	else
	{
		char* pos = memchr(symbol_name.ptr, '/', symbol_name.length);

		if(pos)
		{
			namespace.ptr = symbol_name.ptr;
			namespace.length = pos - symbol_name.ptr;
			symbol.ptr = pos + 1;
			symbol.length = symbol_name.length - namespace.length - 1;
		}
		else
		{
			namespace = lip_string_ref("");
			symbol = symbol_name;
		}
	}

	lip_assert(ctx, lip_rwlock_begin_read(&ctx->runtime->rt_lock));
	bool ret_val = lip_lookup_symbol_locked(ctx, namespace, symbol, result);
	lip_rwlock_end_read(&ctx->runtime->rt_lock);

	return ret_val;
}

static lip_script_t*
lip_do_load_script(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
{
	lip_arena_allocator_reset(ctx->arena_allocator);
	lip_parser_reset(&ctx->parser, input);
	lip_compiler_begin(&ctx->compiler, filename);

	lip_pp_t pp = {
		.allocator = ctx->arena_allocator
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
						ctx->arena_allocator, pp_result.value.result
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
				{
					lip_function_t* fn = lip_compiler_end(&ctx->compiler, ctx->allocator);
					lip_closure_t* closure = lip_new(ctx->allocator, lip_closure_t);
					*closure = (lip_closure_t){
						.function = { .lip = fn },
						.is_native = false,
						.env_len = 0,
					};

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

lip_script_t*
lip_load_script(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
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

	lip_script_t* script = lip_do_load_script(ctx, filename, input);

	if(own_input)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;
		fs->end_read(fs, input);
	}

	return script;
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
(lip_exec_script)(lip_vm_t* vm, lip_script_t* script, lip_value_t* result)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	lip_arena_allocator_reset(rt->allocator);

	return (lip_call)(
		vm,
		result,
		(lip_value_t){
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = script }
		},
		0
	);
}


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
		.allocator = ctx->arena_allocator
	};

	for(;;)
	{
		lip_arena_allocator_reset(ctx->arena_allocator);
		lip_parser_reset(&ctx->parser, &input.vtable);
		ctx->error.num_records = 0;

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
						ctx->arena_allocator, pp_result.value.result
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
					lip_function_t* fn = lip_compiler_end(&ctx->compiler, ctx->arena_allocator);
					lip_closure_t* closure = lip_new(ctx->arena_allocator, lip_closure_t);
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

static void*
lip_retrieve_userdata(khash_t(lip_userdata)* table, void* key)
{
	if(table == NULL) { return NULL; }

	khiter_t itr = kh_get(lip_userdata, table, key);
	return itr == kh_end(table) ? NULL : kh_val(table, itr);
}

static void*
lip_store_userdata(
	khash_t(lip_userdata)** tablep,
	lip_allocator_t* allocator,
	void* key,
	void* value
)
{
	if(*tablep == NULL)
	{
		*tablep = kh_init(lip_userdata, allocator);
	}

	int ret;
	khiter_t itr = kh_put(lip_userdata, *tablep, key, &ret);

	void* old_value = ret == 0 ? kh_val(*tablep, itr) : NULL;
	kh_val(*tablep, itr) = value;
	return old_value;
}

void*
lip_get_userdata(lip_vm_t* vm, lip_userdata_scope_t scope, void* key)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	switch(scope)
	{
		case LIP_SCOPE_VM:
			return lip_retrieve_userdata(rt->userdata, key);
		case LIP_SCOPE_CONTEXT:
			return lip_retrieve_userdata(rt->ctx->userdata, key);
		case LIP_SCOPE_RUNTIME:
			lip_assert(rt->ctx, lip_rwlock_begin_read(&rt->ctx->runtime->rt_lock));
			void* ret = lip_retrieve_userdata(rt->ctx->runtime->userdata, key);
			lip_rwlock_end_read(&rt->ctx->runtime->rt_lock);
			return ret;
		default:
			return NULL;
	}
}

void*
lip_set_userdata(lip_vm_t* vm, lip_userdata_scope_t scope, void* key, void* value)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	switch(scope)
	{
		case LIP_SCOPE_VM:
			return lip_store_userdata(&rt->userdata, rt->allocator, key, value);
		case LIP_SCOPE_CONTEXT:
			return lip_store_userdata(
				&rt->ctx->userdata, rt->ctx->allocator, key, value
			);
		case LIP_SCOPE_RUNTIME:
			lip_assert(rt->ctx, lip_rwlock_begin_write(&rt->ctx->runtime->rt_lock));
			void* ret = lip_store_userdata(
				&rt->ctx->runtime->userdata, rt->ctx->runtime->cfg.allocator,
				key, value
			);
			lip_rwlock_end_write(&rt->ctx->runtime->rt_lock);
			return ret;
		default:
			return NULL;
	}
}
