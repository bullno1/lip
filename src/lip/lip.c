#include "lip_internal.h"
#include <stdio.h>
#include <lip/memory.h>
#include <lip/print.h>
#include "utils.h"

#define LIP_STRING_REF_LITERAL(str) \
	{ .length = LIP_STATIC_ARRAY_LEN(str), .ptr = str}

static lip_string_ref_t LIP_DEFAULT_MODULE_SEARCH_PATTERNS[] = {
	LIP_STRING_REF_LITERAL("?.lip"),
	LIP_STRING_REF_LITERAL("?.lipc"),
	LIP_STRING_REF_LITERAL("!.lip"),
	LIP_STRING_REF_LITERAL("!.lipc"),
	LIP_STRING_REF_LITERAL("?/init.lip"),
	LIP_STRING_REF_LITERAL("?/init.lipc"),
	LIP_STRING_REF_LITERAL("!/init.lip"),
	LIP_STRING_REF_LITERAL("!/init.lipc")
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
		},
		.module_search_patterns = LIP_DEFAULT_MODULE_SEARCH_PATTERNS,
		.num_module_search_patterns = LIP_STATIC_ARRAY_LEN(LIP_DEFAULT_MODULE_SEARCH_PATTERNS)
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
lip_do_destroy_vm(lip_context_t* ctx, lip_vm_t* vm)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
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

	lip_unload_all_scripts(ctx);

	lip_array_destroy(ctx->string_buff);
	kh_destroy(lip_symtab, ctx->loading_symtab);
	kh_destroy(lip_string_ref_set, ctx->loading_modules);
	kh_destroy(lip_ptr_map, ctx->new_exported_functions);
	kh_destroy(lip_ptr_set, ctx->new_script_functions);
	kh_destroy(lip_ptr_set, ctx->vms);
	kh_destroy(lip_ptr_set, ctx->scripts);
	lip_compiler_cleanup(&ctx->compiler);
	lip_parser_cleanup(&ctx->parser);
	lip_array_destroy(ctx->error_records);
	lip_arena_allocator_destroy(ctx->temp_pool);
	lip_arena_allocator_destroy(ctx->module_pool);
	lip_free(ctx->allocator, ctx);
}

void
lip_destroy_runtime(lip_runtime_t* runtime)
{
	kh_foreach(itr, runtime->contexts)
	{
		lip_do_destroy_context(kh_key(runtime->contexts, itr));
	}

	lip_destroy_all_modules(runtime);

	lip_rwlock_destroy(&runtime->rt_lock);
	kh_destroy(lip_symtab, runtime->symtab);
	kh_destroy(lip_ptr_set, runtime->contexts);
	if(runtime->own_fs) { lip_destroy_native_fs(runtime->cfg.fs); }
	lip_free(runtime->cfg.allocator, runtime);
}

void
lip_ctx_begin_rt_read(lip_context_t* ctx)
{
	if(ctx->rt_write_lock_depth > 0)
	{
		++ctx->rt_write_lock_depth;
	}
	else if(++ctx->rt_read_lock_depth == 1)
	{
		lip_assert(ctx, lip_rwlock_begin_read(&ctx->runtime->rt_lock));
	}
}

void
lip_ctx_end_rt_read(lip_context_t* ctx)
{
	if(ctx->rt_write_lock_depth > 0)
	{
		--ctx->rt_write_lock_depth;
	}
	else if(--ctx->rt_read_lock_depth == 0)
	{
		lip_rwlock_end_read(&ctx->runtime->rt_lock);
	}
}

void
lip_ctx_begin_rt_write(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->rt_read_lock_depth == 0);
	if(++ctx->rt_write_lock_depth == 1)
	{
		lip_assert(ctx, lip_rwlock_begin_write(&ctx->runtime->rt_lock));
	}
}

void
lip_ctx_end_rt_write(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->rt_read_lock_depth == 0);
	if(--ctx->rt_write_lock_depth == 0)
	{
		lip_rwlock_end_write(&ctx->runtime->rt_lock);
	}
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
		.temp_pool = lip_arena_allocator_create(allocator, 2048, false),
		.module_pool = lip_arena_allocator_create(allocator, 2048, true),
		.error_records = lip_array_create(allocator, lip_error_record_t, 1),
		.scripts = kh_init(lip_ptr_set, allocator),
		.vms = kh_init(lip_ptr_set, allocator),
		.loading_symtab = kh_init(lip_symtab, allocator),
		.loading_modules = kh_init(lip_string_ref_set, allocator),
		.new_exported_functions = kh_init(lip_ptr_map, allocator),
		.new_script_functions = kh_init(lip_ptr_set, allocator),
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
	if(ctx->last_vm)
	{
		lip_traceback(ctx, ctx->last_vm, ctx->last_result);
		ctx->last_vm = NULL;
	}
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
		return lip_string_ref_from_string(fp->closure->debug_name);
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
	lip_string_ref_t error_message;
	if(msg.type == LIP_VAL_STRING)
	{
		error_message = lip_string_ref_from_string(msg.data.reference);
	}
	else
	{
		lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
		lip_array_clear(rt->ctx->string_buff);
		struct lip_osstream_s osstream;
		lip_out_t* output = lip_make_osstream(&rt->ctx->string_buff, &osstream);
		lip_print_value(1, 0, output, msg);
		size_t msg_len = lip_array_len(rt->ctx->string_buff) - 1; // exclude new line
		error_message = (lip_string_ref_t){
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
				.filename = lip_string_ref_from_string(function_layout.source_name),
				.location = location,
				.message = lip_function_name(fp)
			};
		}
	}
	ctx->error = (lip_context_error_t){
		.message = error_message,
		.num_records = lip_array_len(ctx->error_records),
		.records = ctx->error_records
	};

	return &ctx->error;
}

static void
lip_do_print_error(lip_out_t* out, const lip_context_error_t* err, bool first)
{
	lip_printf(
		out,
		"%s: %.*s.\n",
		first ? "Error" : "caused by",
		(int)err->message.length, err->message.ptr
	);
	for(unsigned int i = 0; i < err->num_records; ++i)
	{
		lip_error_record_t* record = &err->records[i];
		if(memcmp(
			&record->location, &LIP_LOC_NOWHERE, sizeof(LIP_LOC_NOWHERE)
		))
		{
			lip_printf(
				out,
				"  %.*s:%u:%u - %u:%u: %.*s.\n",
				(int)record->filename.length, record->filename.ptr,
				record->location.start.line,
				record->location.start.column,
				record->location.end.line,
				record->location.end.column,
				(int)record->message.length, record->message.ptr
			);
		}
		else
		{
			lip_printf(
				out,
				"  %.*s: %.*s.\n",
				(int)record->filename.length, record->filename.ptr,
				(int)record->message.length, record->message.ptr
			);
		}
	}

	if(err->parent) { lip_do_print_error(out, err->parent, false); }
}

void
lip_print_error(lip_out_t* out, lip_context_t* ctx)
{
	lip_do_print_error(out, lip_get_error(ctx), true);
}

static bool
lip_rt_resolve_import(
	lip_runtime_interface_t* vtable,
	lip_string_t* symbol_name,
	lip_value_t* result
)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vtable, lip_runtime_link_t, vtable);
	lip_assert(rt->ctx, rt->ctx->load_depth > 0);
	lip_string_ref_t symbol_name_ref = lip_string_ref_from_string(symbol_name);
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
		.allocator = lip_arena_allocator_create(ctx->allocator, 2048, true),
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
lip_reset_vm(lip_vm_t* vm)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	lip_arena_allocator_reset(rt->allocator);
	lip_vm_reset(vm);
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

lip_string_ref_t
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
			break;
		case LIP_PARSE_UNEXPECTED_TOKEN:
			return lip_string_ref("Unexpected token");
		case LIP_PARSE_UNTERMINATED_LIST:
			return lip_string_ref("Unterminated list");
	}

	return lip_string_ref("Unknown error");
}

lip_vm_t*
lip_get_default_vm(lip_context_t* ctx)
{
	if(ctx->default_vm == NULL)
	{
		ctx->default_vm = lip_create_vm(ctx, NULL);
	}

	return ctx->default_vm;
}
