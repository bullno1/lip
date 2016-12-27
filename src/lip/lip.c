#include "lip_internal.h"
#include <stdio.h>
#include <lip/array.h>
#include <lip/memory.h>
#include <lip/ast.h>
#include <lip/io.h>

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

lip_runtime_t*
lip_create_runtime(lip_allocator_t* allocator)
{
	lip_runtime_t* runtime = lip_new(allocator, lip_runtime_t);
	*runtime = (lip_runtime_t){
		.allocator = allocator,
		.contexts = kh_init(lip_ptr_set, allocator),
		.symtab = kh_init(lip_symtab, allocator),
	};
	lip_rwlock_init(&runtime->symtab_lock);
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
	lip_arena_allocator_destroy(rt->allocator);
	lip_free(ctx->allocator, vm);
}

static void
lip_do_destroy_context(lip_runtime_t* runtime, lip_context_t* ctx)
{
	(void)runtime;

	kh_foreach(itr, ctx->vms)
	{
		lip_do_destroy_vm(ctx, kh_key(ctx->vms, itr));
	}

	kh_foreach(itr, ctx->scripts)
	{
		lip_do_unload_script(ctx, kh_key(ctx->scripts, itr));
	}

	kh_destroy(lip_ptr_set, ctx->vms);
	kh_destroy(lip_ptr_set, ctx->scripts);
	lip_compiler_cleanup(&ctx->compiler);
	lip_parser_cleanup(&ctx->parser);
	lip_array_destroy(ctx->error_records);
	lip_arena_allocator_destroy(ctx->arena_allocator);
	lip_free(ctx->allocator, ctx);
}

static void
lip_purge_ns(lip_runtime_t* runtime, khash_t(lip_ns)* ns)
{
	kh_foreach(i, ns)
	{
		lip_free(runtime->allocator, (void*)kh_key(ns, i).ptr);
		lip_free(runtime->allocator, kh_val(ns, i).value);
	}
	kh_clear(lip_ns, ns);
}

void
lip_destroy_runtime(lip_runtime_t* runtime)
{
	kh_foreach(itr, runtime->contexts)
	{
		lip_do_destroy_context(runtime, kh_key(runtime->contexts, itr));
	}

	kh_foreach(itr, runtime->symtab)
	{
		lip_free(runtime->allocator, (void*)kh_key(runtime->symtab, itr).ptr);
		khash_t(lip_ns)* ns = kh_val(runtime->symtab, itr);
		lip_purge_ns(runtime, ns);
		kh_destroy(lip_ns, ns);
	}

	lip_rwlock_destroy(&runtime->symtab_lock);
	kh_destroy(lip_symtab, runtime->symtab);
	kh_destroy(lip_ptr_set, runtime->contexts);
	lip_free(runtime->allocator, runtime);
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
	if(allocator == NULL) { allocator = runtime->allocator; }

	lip_context_t* ctx = lip_new(allocator, lip_context_t);
	*ctx = (lip_context_t) {
		.allocator = allocator,
		.runtime = runtime,
		.arena_allocator = lip_arena_allocator_create(allocator, 2048),
		.error_records = lip_array_create(allocator, lip_error_record_t, 1),
		.scripts = kh_init(lip_ptr_set, allocator),
		.vms = kh_init(lip_ptr_set, allocator),
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
		void* str_buff = lip_malloc(runtime->allocator, ns_ctx->name.length);
		memcpy(str_buff, ns_ctx->name.ptr, ns_ctx->name.length);
		lip_string_ref_t ns_name = {.ptr = str_buff, .length = ns_ctx->name.length};
		ns = kh_init(lip_ns, runtime->allocator);

		int ret;
		khiter_t itr = kh_put(lip_symtab, symtab, ns_name, &ret);
		kh_val(symtab, itr) = ns;
	}

	kh_foreach(i, ns_ctx->content)
	{
		lip_string_ref_t symbol_name = kh_key(ns_ctx->content, i);
		lip_symbol_t symbol_value = kh_val(ns_ctx->content, i);

		void* str_buff = lip_malloc(runtime->allocator, symbol_name.length);
		memcpy(str_buff, symbol_name.ptr, symbol_name.length);
		symbol_name.ptr = str_buff;

		size_t closure_size =
			sizeof(lip_closure_t) +
			sizeof(lip_value_t) * symbol_value.value->env_len;
		lip_closure_t* closure = lip_malloc(runtime->allocator, closure_size);
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
	lip_assert(ctx, lip_rwlock_begin_write(&ctx->runtime->symtab_lock));
	lip_commit_ns_locked(ctx, ns);
	lip_rwlock_end_write(&ctx->runtime->symtab_lock);

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

static lip_closure_t*
lip_rt_alloc_closure(lip_runtime_interface_t* vtable, uint8_t env_len)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vtable, lip_runtime_link_t, vtable);
	return lip_malloc(
		rt->allocator, sizeof(lip_closure_t) + sizeof(lip_value_t) * env_len
	);
}

lip_vm_t*
lip_create_vm(lip_context_t* ctx, lip_vm_config_t* config)
{
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
			.alloc_closure = lip_rt_alloc_closure
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
lip_set_compile_error(
	lip_context_t* ctx,
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
	ctx->error.message = lip_string_ref("Syntax error");
	ctx->error.num_records = 1;
	ctx->error.records = ctx->error_records;
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
	char* pos = memchr(symbol_name.ptr, '/', symbol_name.length);
	lip_string_ref_t namespace;
	lip_string_ref_t symbol;

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

	lip_assert(ctx, lip_rwlock_begin_read(&ctx->runtime->symtab_lock));
	bool ret_val = lip_lookup_symbol_locked(ctx, namespace, symbol, result);
	lip_rwlock_end_read(&ctx->runtime->symtab_lock);

	return ret_val;
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
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	lip_arena_allocator_reset(rt->allocator);

	return lip_call(
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
lip_repl(lip_vm_t* vm, lip_repl_handler_t* repl_handler)
{
	lip_string_ref_t source_name = lip_string_ref("repl");

	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	lip_context_t* ctx = rt->ctx;

	struct lip_repl_stream_s input = {
		.repl_handler = repl_handler,
		.vtable = { .read = lip_repl_read }
	};

	for(;;)
	{
		lip_arena_allocator_reset(ctx->arena_allocator);
		lip_parser_reset(&ctx->parser, &input.vtable);

		lip_sexp_t sexp;
		switch(lip_parser_next_sexp(&ctx->parser, &sexp))
		{
			case LIP_STREAM_OK:
				{
					lip_ast_result_t ast_result =
						lip_translate_sexp(ctx->arena_allocator, &sexp);
					if(ast_result.success)
					{
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
					else
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
					}
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
