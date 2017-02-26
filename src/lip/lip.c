#include "lip_internal.h"
#include <stdio.h>
#include <lip/array.h>
#include <lip/memory.h>
#include <lip/pp.h>
#include <lip/ast.h>
#include <lip/io.h>
#include <lip/vm.h>
#include <lip/print.h>
#include <lip/asm.h>
#include "utils.h"

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
#define LIP_STRING_REF_LITERAL(str) \
	{ .length = LIP_STATIC_ARRAY_LEN(str), .ptr = str}

static const char LIP_BINARY_MAGIC[] = {'L', 'I', 'P', 0};
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
const char lip_module_ctx_key = 0;

typedef struct lip_repl_stream_s lip_repl_stream_t;

struct lip_repl_stream_s
{
	lip_in_t vtable;
	lip_repl_handler_t* repl_handler;
};

struct lip_prefix_stream_s
{
	lip_in_t vtable;
	lip_in_t* inner_stream;
	char* prefix;
	size_t prefix_len;
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
lip_do_unload_script(lip_context_t* ctx, lip_script_t* script)
{
	lip_closure_t* closure = script->closure;
	lip_free(ctx->allocator, closure->function.lip);
	lip_free(ctx->allocator, closure);
	lip_free(ctx->allocator, script);
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
	kh_destroy(lip_symtab, ctx->loading_symtab);
	kh_destroy(lip_string_ref_set, ctx->loading_modules);
	kh_destroy(lip_ptr_set, ctx->new_exported_functions);
	kh_destroy(lip_ptr_set, ctx->new_script_functions);
	kh_destroy(lip_ptr_set, ctx->vms);
	kh_destroy(lip_ptr_set, ctx->scripts);
	lip_compiler_cleanup(&ctx->compiler);
	lip_parser_cleanup(&ctx->parser);
	lip_array_destroy(ctx->error_records);
	lip_arena_allocator_destroy(ctx->temp_pool);
	lip_arena_allocator_destroy(ctx->module_pool);
	if(ctx->userdata) { kh_destroy(lip_userdata, ctx->userdata); }
	lip_free(ctx->allocator, ctx);
}

static void
lip_purge_ns(lip_runtime_t* runtime, khash_t(lip_ns)* ns)
{
	kh_foreach(i, ns)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(ns, i).ptr);
		lip_closure_t* closure = kh_val(ns, i).value;
		lip_free(runtime->cfg.allocator, closure->debug_name);
		if(!closure->is_native)
		{
			lip_free(runtime->cfg.allocator, closure->function.lip);
		}
		lip_free(runtime->cfg.allocator, closure);
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

static void
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

static void
lip_ctx_begin_rt_write(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->rt_read_lock_depth == 0);
	if(++ctx->rt_write_lock_depth == 1)
	{
		lip_assert(ctx, lip_rwlock_begin_write(&ctx->runtime->rt_lock));
	}
}

static void
lip_ctx_end_rt_write(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->rt_read_lock_depth == 0);
	if(--ctx->rt_write_lock_depth == 0)
	{
		lip_rwlock_end_write(&ctx->runtime->rt_lock);
	}
}

static void
lip_ctx_begin_load(lip_context_t* ctx)
{
	lip_ctx_begin_rt_write(ctx);
	if(++ctx->load_depth == 1)
	{
		lip_arena_allocator_reset(ctx->module_pool);
		kh_clear(lip_symtab, ctx->loading_symtab);
		kh_clear(lip_string_ref_set, ctx->loading_modules);
		kh_clear(lip_ptr_set, ctx->new_exported_functions);
		kh_clear(lip_ptr_set, ctx->new_script_functions);
		ctx->last_vm = NULL;
	}
}

static lip_closure_t*
lip_copy_closure(lip_allocator_t* allocator, lip_closure_t* closure)
{
	size_t closure_size =
		sizeof(lip_closure_t) +
		sizeof(lip_value_t) * closure->env_len;
	lip_closure_t* closure_copy = lip_malloc(allocator, closure_size);
	memcpy(closure_copy, closure, closure_size);

	if(!closure->is_native)
	{
		lip_function_t* function = closure->function.lip;
		lip_function_t* function_copy = lip_malloc(allocator, function->size);
		memcpy(function_copy, function, function->size);
		closure_copy->function.lip = function_copy;
	}

	return closure_copy;
}

static void
lip_commit_ns_locked(lip_context_t* ctx, lip_string_ref_t name, khash_t(lip_ns)* ns)
{
	lip_assert(ctx, ctx->rt_write_lock_depth > 0);
	lip_runtime_t* runtime = ctx->runtime;
	khash_t(lip_symtab)* symtab = runtime->symtab;
	khiter_t itr = kh_get(lip_symtab, symtab, name);

	khash_t(lip_ns)* target_ns;
	if(itr != kh_end(symtab))
	{
		target_ns = kh_val(symtab, itr);
		lip_purge_ns(runtime, target_ns);
	}
	else
	{
		lip_string_ref_t ns_name = lip_copy_string_ref(runtime->cfg.allocator, name);
		target_ns = kh_init(lip_ns, runtime->cfg.allocator);

		int ret;
		khiter_t itr = kh_put(lip_symtab, symtab, ns_name, &ret);
		kh_val(symtab, itr) = target_ns;
	}

	kh_foreach(i, ns)
	{
		lip_string_ref_t key = kh_key(ns, i);
		lip_symbol_t value = kh_val(ns, i);

		lip_string_ref_t symbol_name = lip_copy_string_ref(runtime->cfg.allocator, key);
		value.value = lip_copy_closure(runtime->cfg.allocator, value.value);

		lip_arena_allocator_reset(ctx->temp_pool);
		lip_array(char) fqn_buf = lip_array_create(ctx->temp_pool, char, 64);
		lip_sprintf(
			&fqn_buf,
			"%.*s/%.*s",
			(int)name.length, name.ptr,
			(int)key.length, key.ptr
		);
		size_t str_len = lip_array_len(fqn_buf);
		lip_string_t* debug_name = lip_malloc(
			runtime->cfg.allocator, sizeof(lip_string_t) + str_len + 1
		);
		memcpy(debug_name->ptr, fqn_buf, str_len);
		debug_name->ptr[str_len] = '\0';
		debug_name->length = str_len;
		value.value->debug_name = debug_name;

		int ret;
		if(!value.value->is_native)
		{
			kh_put(lip_ptr_set,
				ctx->new_exported_functions, value.value->function.lip, &ret
			);
		}

		khiter_t itr = kh_put(lip_ns, target_ns, symbol_name, &ret);
		kh_val(target_ns, itr) = value;
	}
}

static void
lip_split_fqn(
	lip_string_ref_t fqn,
	lip_string_ref_t* module_name,
	lip_string_ref_t* symbol_name
)
{
	if(fqn.length == 1 && fqn.ptr[0] == '/')
	{
		*module_name = lip_string_ref("");
		*symbol_name = fqn;
	}
	else
	{
		char* pos = memchr(fqn.ptr, '/', fqn.length);

		if(pos)
		{
			module_name->ptr = fqn.ptr;
			module_name->length = pos - fqn.ptr;
			symbol_name->ptr = pos + 1;
			symbol_name->length = fqn.length - module_name->length - 1;
		}
		else
		{
			*module_name = lip_string_ref("");
			*symbol_name = fqn;
		}
	}
}

static bool
lip_import_referenced(
	lip_function_t* fn, uint16_t import_index, lip_loc_range_t* loc
)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	for(uint16_t i = 0; i < fn->num_instructions; ++i)
	{
		lip_instruction_t instr = layout.instructions[i];
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(instr, &opcode, &operand);
		if(opcode == LIP_OP_IMP && operand == import_index)
		{
			*loc = layout.locations[i];
			return true;
		}
	}

	return false;
}

static void
lip_static_link_function(lip_context_t* ctx, lip_function_t* fn)
{
	lip_assert(ctx, ctx->rt_read_lock_depth + ctx->rt_write_lock_depth > 0);
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	for(uint16_t i = 0; i < fn->num_imports; ++i)
	{
		// Some imports are replaced with builtins ops
		lip_loc_range_t loc;
		if(!lip_import_referenced(fn, i, &loc)) { continue; }

		lip_string_t* name = lip_function_resource(fn, layout.imports[i].name);
		lip_string_ref_t symbol_name_ref = { .length = name->length, .ptr = name->ptr };

		lip_assert(ctx, lip_lookup_symbol(ctx, symbol_name_ref, &layout.imports[i].value));
	}

	for(uint16_t i = 0; i < fn->num_instructions; ++i)
	{
		lip_instruction_t* instr = &layout.instructions[i];
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(*instr, &opcode, &operand);

		if(opcode == LIP_OP_IMP)
		{
			*instr = lip_asm(LIP_OP_IMPS, operand);
		}
	}

	for(uint16_t i = 0; i < fn->num_functions; ++i)
	{
		lip_static_link_function(
			ctx, lip_function_resource(fn, layout.function_offsets[i])
		);
	}
}

static void
lip_ctx_end_load(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->load_depth > 0);
	if(--ctx->load_depth == 0)
	{
		kh_foreach(itr, ctx->loading_symtab)
		{
			lip_commit_ns_locked(
				ctx,
				kh_key(ctx->loading_symtab, itr),
				kh_val(ctx->loading_symtab, itr)
			);
		}

		kh_foreach(itr, ctx->new_exported_functions)
		{
			lip_static_link_function(ctx, kh_key(ctx->new_exported_functions, itr));
		}

		kh_foreach(itr, ctx->new_script_functions)
		{
			lip_static_link_function(ctx, kh_key(ctx->new_script_functions, itr));
		}
	}

	lip_ctx_end_rt_write(ctx);
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
		.new_exported_functions = kh_init(lip_ptr_set, allocator),
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

void
lip_print_error(lip_out_t* out, lip_context_t* ctx)
{
	const lip_context_error_t* err = lip_get_error(ctx);
	lip_printf(
		out,
		"Error: %.*s.\n",
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
}

lip_ns_context_t*
lip_begin_ns(lip_context_t* ctx, lip_string_ref_t name)
{
	lip_ns_context_t* ns_context = lip_new(ctx->allocator, lip_ns_context_t);
	*ns_context = (lip_ns_context_t){
		.allocator = lip_arena_allocator_create(ctx->allocator, 2048, true),
		.content = kh_init(lip_ns, ctx->allocator),
		.name = name
	};
	return ns_context;
}

void
lip_end_ns(lip_context_t* ctx, lip_ns_context_t* ns)
{
	lip_ctx_begin_load(ctx);
	lip_commit_ns_locked(ctx, ns->name, ns->content);
	lip_ctx_end_load(ctx);

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
	lip_assert(rt->ctx, rt->ctx->load_depth > 0);
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
	lip_string_ref_t module, symbol;

	lip_split_fqn(symbol_name, &module, &symbol);

	lip_ctx_begin_rt_read(ctx);
	bool ret_val = lip_lookup_symbol_locked(ctx, module, symbol, result);
	lip_ctx_end_rt_read(ctx);

	return ret_val;
}

static void
lip_set_undefined_symbol_error(
	lip_context_t* ctx,
	lip_function_t* fn,
	lip_string_t* name,
	lip_loc_range_t loc
)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	lip_array(char) msg_buf = lip_array_create(ctx->module_pool, char, 64);
	lip_sprintf(&msg_buf, "Undefined symbol: %.*s", (int)name->length, name->ptr);
	lip_array_push(msg_buf, '\0');
	lip_array_clear(ctx->error_records);
	lip_error_record_t record = {
		.filename = lip_copy_string_ref(
			ctx->module_pool, lip_string_ref_from_string(layout.source_name)
		),
		.location = loc,
		.message = lip_string_ref("Referenced here")
	};
	lip_array_push(ctx->error_records, record);

	ctx->error = (lip_context_error_t){
		.message = {
			.length = lip_array_len(msg_buf) - 1,
			.ptr = msg_buf
		},
		.records = ctx->error_records,
		.num_records = lip_array_len(ctx->error_records)
	};
}

static bool
lip_link_function(lip_context_t* ctx, lip_function_t* fn)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	for(uint16_t i = 0; i < fn->num_imports; ++i)
	{
		// Some imports are replaced with builtins ops
		lip_loc_range_t loc;
		if(!lip_import_referenced(fn, i, &loc)) { continue; }

		lip_string_t* name = lip_function_resource(fn, layout.imports[i].name);
		lip_string_ref_t module_name, function_name;
		lip_string_ref_t symbol_name_ref = { .length = name->length, .ptr = name->ptr };
		lip_split_fqn(symbol_name_ref, &module_name, &function_name);

		khiter_t itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
		if(itr != kh_end(ctx->loading_symtab))
		{
			khash_t(lip_ns)* ns = kh_val(ctx->loading_symtab, itr);
			if(kh_get(lip_ns, ns, function_name) != kh_end(ns))
			{
				continue;
			}
			else
			{
				lip_set_undefined_symbol_error(ctx, fn, name, loc);
				return false;
			}
		}

		itr = kh_get(lip_symtab, ctx->runtime->symtab, module_name);
		if(itr != kh_end(ctx->runtime->symtab))
		{
			khash_t(lip_ns)* ns = kh_val(ctx->runtime->symtab, itr);
			if(kh_get(lip_ns, ns, function_name) != kh_end(ns))
			{
				continue;
			}
			else
			{
				lip_set_undefined_symbol_error(ctx, fn, name, loc);
				return false;
			}
		}

		if(!lip_load_module(ctx, module_name)) { return false; }

		itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
		lip_assert(ctx, itr != kh_end(ctx->loading_symtab));
		khash_t(lip_ns)* ns = kh_val(ctx->loading_symtab, itr);
		if(kh_get(lip_ns, ns, function_name) == kh_end(ns))
		{
			lip_set_undefined_symbol_error(ctx, fn, name, loc);
			return false;
		}
	}

	for(uint16_t i = 0; i < fn->num_functions; ++i)
	{
		bool link_status = lip_link_function(
			ctx, lip_function_resource(fn, layout.function_offsets[i])
		);
		if(!link_status) { return false; }
	}

	return true;
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
lip_do_load_script(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input)
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

lip_script_t*
lip_load_script(
	lip_context_t* ctx,
	lip_string_ref_t filename,
	lip_in_t* input,
	bool link
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

	lip_ctx_begin_load(ctx);

	lip_function_t* fn = lip_do_load_script(ctx, filename, input);
	lip_script_t* script = NULL;
	if(fn)
	{
		if(link && !lip_link_function(ctx, fn))
		{
			lip_free(ctx->allocator, fn);
			return NULL;
		}

		lip_closure_t* closure = lip_new(ctx->allocator, lip_closure_t);
		*closure = (lip_closure_t){
			.function = { .lip = fn },
			.is_native = false,
			.env_len = 0,
		};
		int ret;
		if(link) { kh_put(lip_ptr_set, ctx->new_script_functions, fn, &ret); }

		script = lip_new(ctx->allocator, lip_script_t);
		*script = (lip_script_t){
			.closure = closure,
			.linked = link
		};
		kh_put(lip_ptr_set, ctx->scripts, script, &ret);
	}

	lip_ctx_end_load(ctx);

	if(own_input)
	{
		lip_fs_t* fs = ctx->runtime->cfg.fs;
		fs->end_read(fs, input);
	}

	return script;
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

	itr = kh_get(lip_ptr_set, ctx->scripts, script);
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

	if(!script->linked)
	{
		lip_function_t* fn = script->closure->function.lip;
		lip_ctx_begin_load(rt->ctx);
		bool link_result = lip_link_function(rt->ctx, fn);
		if(link_result)
		{
			int ret;
			kh_put(lip_ptr_set, rt->ctx->new_script_functions, fn, &ret);
		}
		lip_ctx_end_load(rt->ctx);
		script->linked = link_result;
		if(!link_result) { return LIP_EXEC_ERROR; }
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

static lip_vm_t*
lip_get_internal_vm(lip_context_t* ctx)
{
	if(ctx->internal_vm == NULL)
	{
		ctx->internal_vm = lip_create_vm(ctx, NULL);
	}

	return ctx->internal_vm;
}

bool
lip_load_module(lip_context_t* ctx, lip_string_ref_t name)
{
	int ret;
	kh_put(lip_string_ref_set, ctx->loading_modules, name, &ret);
	if(ret == 0) { return false; }

	bool result = false;
	lip_ctx_begin_load(ctx);
#define returnVal(X) do { result = X; goto end; } while(0)

	unsigned int num_patterns = ctx->runtime->cfg.num_module_search_patterns;
	const lip_string_ref_t* patterns = ctx->runtime->cfg.module_search_patterns;
	lip_fs_t* fs = ctx->runtime->cfg.fs;
	lip_in_t* file = NULL;
	lip_script_t* script = NULL;
	lip_string_ref_t filename;

	lip_array_clear(ctx->error_records);

	for(unsigned int i = 0; i < num_patterns; ++i)
	{
		lip_array(char) filename_buf = lip_array_create(ctx->module_pool, char, 512);
		lip_string_ref_t pattern = patterns[i];

		for(unsigned int j = 0; j < pattern.length; ++j)
		{
			char pattern_ch = pattern.ptr[j];
			switch(pattern_ch)
			{
				case '?':
					for(unsigned int k = 0; k < name.length; ++k)
					{
						char name_ch = name.ptr[k];
						if(name_ch == '.') { name_ch = '/'; }
						lip_array_push(filename_buf, name_ch);
					}
					break;
				case '!':
					{
						size_t array_len = lip_array_len(filename_buf);
						lip_array_resize(filename_buf, array_len + name.length);
						memcpy(filename_buf + array_len, name.ptr, name.length);
					}
					break;
				default:
					lip_array_push(filename_buf, pattern_ch);
					break;
			}
		}

		size_t str_len = lip_array_len(filename_buf);
		lip_array_push(filename_buf, 0);
		filename = (lip_string_ref_t){ .length = str_len, .ptr = filename_buf };

		file = fs->begin_read(fs, filename);
		if(file == NULL)
		{
			lip_string_ref_t error = fs->last_error(fs);
			lip_string_ref_t error_copy = lip_copy_string_ref(ctx->module_pool, error);
			lip_error_record_t record = {
				.filename = filename,
				.location = LIP_LOC_NOWHERE,
				.message = error_copy
			};
			lip_array_push(ctx->error_records, record);
		}
		else
		{
			break;
		}
	}

	if(file == NULL)
	{
		lip_array(char) error_msg_buf = lip_array_create(ctx->module_pool, char, 64);
		lip_sprintf(
			&error_msg_buf,
			"Could not load module %.*s", (int)name.length, name.ptr
		);
		lip_array_push(error_msg_buf, '\0');
		ctx->error = (lip_context_error_t){
			.message = {
				.length = lip_array_len(error_msg_buf),
				.ptr = error_msg_buf
			},
			.num_records = lip_array_len(ctx->error_records),
			.records = ctx->error_records
		};
		returnVal(false);
	}

	script = lip_load_script(ctx, filename, file, true);
	if(script == NULL) { returnVal(false); }

	if(!lip_link_function(ctx, script->closure->function.lip))
	{
		returnVal(false);
	}

	lip_vm_t* vm = lip_get_internal_vm(ctx);
	khash_t(lip_ns)* module = kh_init(lip_ns, ctx->module_pool);
	lip_string_ref_t name_copy = lip_copy_string_ref(ctx->module_pool, name);
	khiter_t itr = kh_put(lip_symtab, ctx->loading_symtab, name_copy, &ret);
	kh_val(ctx->loading_symtab, itr) = module;
	void* previous_ctx = lip_set_userdata(vm, LIP_SCOPE_CONTEXT, &lip_module_ctx_key, module);
	lip_value_t exec_result;
	lip_exec_status_t status =
		lip_exec_script(lip_get_internal_vm(ctx), script, &exec_result);
	lip_set_userdata(vm, LIP_SCOPE_CONTEXT, &lip_module_ctx_key, previous_ctx);

	if(status == LIP_EXEC_OK)
	{
		// Copy out all declared name-function pairs because the script is going
		// to be freed
		kh_foreach(itr, module)
		{
			lip_string_ref_t* key = &kh_key(module, itr);
			lip_symbol_t* value = &kh_val(module, itr);

			value->value = lip_copy_closure(ctx->module_pool, value->value);
			*key = lip_copy_string_ref(ctx->module_pool, *key);
		}
		returnVal(true);
	}
	else
	{
		returnVal(false);
	}

end:
	if(file) { fs->end_read(fs, file); }
	if(script) { lip_unload_script(ctx, script); }

	lip_ctx_end_load(ctx);
	return result;
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
		.allocator = ctx->temp_pool
	};

	for(;;)
	{
		lip_arena_allocator_reset(ctx->temp_pool);
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

static void*
lip_retrieve_userdata(khash_t(lip_userdata)* table, const void* key)
{
	if(table == NULL) { return NULL; }

	khiter_t itr = kh_get(lip_userdata, table, key);
	return itr == kh_end(table) ? NULL : kh_val(table, itr);
}

static void*
lip_store_userdata(
	khash_t(lip_userdata)** tablep,
	lip_allocator_t* allocator,
	const void* key,
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
lip_get_userdata(lip_vm_t* vm, lip_userdata_scope_t scope, const void* key)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	switch(scope)
	{
		case LIP_SCOPE_VM:
			return lip_retrieve_userdata(rt->userdata, key);
		case LIP_SCOPE_CONTEXT:
			return lip_retrieve_userdata(rt->ctx->userdata, key);
		case LIP_SCOPE_RUNTIME:
			lip_ctx_begin_rt_read(rt->ctx);
			void* ret = lip_retrieve_userdata(rt->ctx->runtime->userdata, key);
			lip_ctx_end_rt_read(rt->ctx);
			return ret;
		default:
			return NULL;
	}
}

void*
lip_set_userdata(
	lip_vm_t* vm, lip_userdata_scope_t scope, const void* key, void* value
)
{
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);
	switch(scope)
	{
		case LIP_SCOPE_VM:
			return lip_store_userdata(&rt->userdata, rt->ctx->allocator, key, value);
		case LIP_SCOPE_CONTEXT:
			return lip_store_userdata(
				&rt->ctx->userdata, rt->ctx->allocator, key, value
			);
		case LIP_SCOPE_RUNTIME:
			lip_ctx_begin_rt_write(rt->ctx);
			void* ret = lip_store_userdata(
				&rt->ctx->runtime->userdata, rt->ctx->runtime->cfg.allocator,
				key, value
			);
			lip_ctx_end_rt_write(rt->ctx);
			return ret;
		default:
			return NULL;
	}
}
