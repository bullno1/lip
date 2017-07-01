#include "lip_internal.h"
#include <lip/asm.h>
#include "utils.h"

typedef bool(*lip_import_iteratee_t)(
	lip_function_t* fn,
	lip_import_t* import,
	lip_loc_range_t loc,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name,
	void* ctx
);

typedef bool(*lip_function_iteratee_t)(
	lip_function_t* fn,
	lip_function_layout_t* layout,
	void* ctx
);

struct lip_link_ctx_s
{
	lip_context_t* ctx;
	khash_t(lip_module)* module;
	lip_function_t* top_level_fn;
};

struct lip_import_itr_ctx_s
{
	void* user_ctx;
	lip_import_iteratee_t iteratee;
};

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
			*loc = layout.locations[i + 1]; // slot 0 is function's location
			return true;
		}
	}

	return false;
}

static lip_symbol_t*
lip_lookup_symbol_in_symtab(
	khash_t(lip_symtab)* symtab,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name
)
{
	khiter_t itr = kh_get(lip_symtab, symtab, module_name);
	if(itr == kh_end(symtab)) { return false; }

	khash_t(lip_module)* module = kh_val(symtab, itr);
	itr = kh_get(lip_module, module, symbol_name);
	if(itr == kh_end(module)) { return false; }

	return &kh_val(module, itr);
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

static void
lip_purge_module(lip_runtime_t* runtime, khash_t(lip_module)* module)
{
	kh_foreach(i, module)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(module, i).ptr);
		lip_closure_t* closure = kh_val(module, i).value;
		lip_free(runtime->cfg.allocator, closure->debug_name);
		if(!closure->is_native)
		{
			lip_free(runtime->cfg.allocator, closure->function.lip);
		}
		lip_free(runtime->cfg.allocator, closure);
	}
	kh_clear(lip_module, module);
}

static void
lip_commit_module_locked(lip_context_t* ctx, lip_string_ref_t name, khash_t(lip_module)* module)
{
	lip_assert(ctx, ctx->rt_write_lock_depth > 0);
	lip_runtime_t* runtime = ctx->runtime;
	khash_t(lip_symtab)* symtab = runtime->symtab;
	khiter_t itr = kh_get(lip_symtab, symtab, name);

	khash_t(lip_module)* target_module;
	if(itr != kh_end(symtab))
	{
		target_module = kh_val(symtab, itr);
		lip_purge_module(runtime, target_module);
	}
	else
	{
		lip_string_ref_t module_name = lip_copy_string_ref(runtime->cfg.allocator, name);
		target_module = kh_init(lip_module, runtime->cfg.allocator);

		int ret;
		khiter_t itr = kh_put(lip_symtab, symtab, module_name, &ret);
		kh_val(symtab, itr) = target_module;
	}

	kh_foreach(i, module)
	{
		lip_string_ref_t key = kh_key(module, i);
		lip_symbol_t value = kh_val(module, i);

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
			khiter_t itr = kh_put(lip_ptr_map,
				ctx->new_exported_functions, value.value->function.lip, &ret
			);
			kh_val(ctx->new_exported_functions, itr) = target_module;
		}

		khiter_t itr = kh_put(lip_module, target_module, symbol_name, &ret);
		kh_val(target_module, itr) = value;
	}
}

static bool
lip_lookup_symbol_locked(
	lip_context_t* ctx,
	lip_string_ref_t module,
	lip_string_ref_t symbol_name,
	lip_value_t* result
)
{
	lip_symbol_t* symbol = NULL;

	if(ctx->current_module)
	{
		khiter_t itr = kh_get(lip_module, ctx->current_module, symbol_name);
		if(itr != kh_end(ctx->current_module))
		{
			symbol = &kh_val(ctx->current_module, itr);
		}
	}

	if(symbol == NULL)
	{
		lip_lookup_symbol_in_symtab(ctx->loading_symtab, module, symbol_name);
	}

	if(symbol == NULL)
	{
		symbol = lip_lookup_symbol_in_symtab(
			ctx->runtime->symtab, module, symbol_name
		);
	}

	if(symbol == NULL) { return false; }

	*result = (lip_value_t){
		.type = LIP_VAL_FUNCTION,
		.data = { .reference = symbol->value }
	};
	return true;
}

static void
lip_ctx_abort_load(lip_context_t* ctx)
{
	ctx->load_aborted = true;
}

static void
lip_set_link_error(
	lip_context_t* ctx,
	lip_string_ref_t error_message,
	lip_function_t* fn,
	lip_loc_range_t loc,
	bool stack
)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	lip_error_record_t* error_record = lip_new(ctx->module_pool, lip_error_record_t);
	*error_record = (lip_error_record_t){
		.filename = lip_copy_string_ref(
			ctx->module_pool, lip_string_ref_from_string(layout.source_name)
		),
		.location = loc,
		.message = lip_string_ref("Referenced here")
	};

	if(stack)
	{
		lip_context_error_t* error_copy =
				lip_new(ctx->module_pool, lip_context_error_t);
		*error_copy = ctx->error;
		ctx->error = (lip_context_error_t){
			.message = error_message,
			.records = error_record,
			.num_records = 1,
			.parent = error_copy
		};
	}
	else
	{
		ctx->error = (lip_context_error_t){
			.message = error_message,
			.records = error_record,
			.num_records = 1
		};
	}

	lip_ctx_abort_load(ctx);
}

static void
lip_set_undefined_symbol_error(
	lip_context_t* ctx,
	lip_function_t* fn,
	lip_string_t* name,
	lip_loc_range_t loc,
	bool stack
)
{
	lip_array(char) msg_buf = lip_array_create(ctx->module_pool, char, 64);
	lip_sprintf(&msg_buf, "Undefined symbol: %.*s", (int)name->length, name->ptr);
	lip_array_push(msg_buf, '\0');
	lip_string_ref_t error_message = {
		.length = lip_array_len(msg_buf) - 1,
		.ptr = msg_buf
	};
	lip_set_link_error(ctx, error_message, fn, loc, stack);
}

lip_module_context_t*
lip_begin_module(lip_context_t* ctx, lip_string_ref_t name)
{
	lip_ctx_begin_load(ctx);
	lip_module_context_t* module_context = lip_new(ctx->module_pool, lip_module_context_t);
	*module_context = (lip_module_context_t){
		.content = kh_init(lip_module, ctx->module_pool),
		.allocator = ctx->module_pool,
		.name = name
	};
	return module_context;
}

void
lip_end_module(lip_context_t* ctx, lip_module_context_t* module)
{
	int ret;
	khiter_t itr = kh_put(lip_symtab, ctx->loading_symtab, module->name, &ret);
	kh_val(ctx->loading_symtab, itr) = module->content;

	lip_ctx_end_load(ctx);
}

void
lip_discard_module(lip_context_t* ctx, lip_module_context_t* module)
{
	(void)module;
	lip_ctx_end_load(ctx);
}

void
lip_declare_function(
	lip_module_context_t* module, lip_string_ref_t name, lip_native_fn_t fn
)
{
	int ret;
	khiter_t itr  = kh_put(lip_module, module->content, name, &ret);

	lip_closure_t* closure = lip_new(module->allocator, lip_closure_t);
	*closure = (lip_closure_t){
		.is_native = true,
		.env_len = 0,
		.function = { .native = fn }
	};

	kh_val(module->content, itr) = (lip_symbol_t) {
		.is_public = true,
		.value = closure
	};
}

void
lip_ctx_begin_load(lip_context_t* ctx)
{
	lip_ctx_begin_rt_write(ctx);
	if(++ctx->load_depth == 1)
	{
		lip_arena_allocator_reset(ctx->module_pool);
		kh_clear(lip_symtab, ctx->loading_symtab);
		kh_clear(lip_string_ref_set, ctx->loading_modules);
		kh_clear(lip_ptr_map, ctx->new_exported_functions);
		kh_clear(lip_ptr_set, ctx->new_script_functions);
		ctx->last_vm = NULL;
		ctx->load_aborted = false;
	}
}

void
lip_destroy_all_modules(lip_runtime_t* runtime)
{
	kh_foreach(itr, runtime->symtab)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(runtime->symtab, itr).ptr);
		khash_t(lip_module)* module = kh_val(runtime->symtab, itr);
		lip_purge_module(runtime, module);
		kh_destroy(lip_module, module);
	}
}

static bool
lip_iterate_functions(
	lip_function_t* fn,
	lip_function_iteratee_t iteratee,
	void* ctx
)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);
	if(!iteratee(fn, &layout, ctx)) { return false; }

	for(uint16_t i = 0; i < fn->num_functions; ++i)
	{
		lip_function_t* nested_fn =
			lip_function_resource(fn, layout.function_offsets[i]);
		if(!lip_iterate_functions(nested_fn, iteratee, ctx)) { return false; }
	}

	return true;
}

static bool
lip_apply_import_iteratee(
	lip_function_t* fn,
	lip_function_layout_t* layout,
	void* ctx_
)
{
	struct lip_import_itr_ctx_s* ctx = ctx_;

	for(uint16_t i = 0; i < fn->num_imports; ++i)
	{
		// Some imports are replaced with builtins ops
		lip_loc_range_t loc;
		if(!lip_import_referenced(fn, i, &loc)) { continue; }

		lip_string_t* name = lip_function_resource(fn, layout->imports[i].name);
		lip_string_ref_t module_name, function_name;
		lip_string_ref_t symbol_name_ref = { .length = name->length, .ptr = name->ptr };
		lip_split_fqn(symbol_name_ref, &module_name, &function_name);

		if(!ctx->iteratee(
			fn, &layout->imports[i], loc, module_name, function_name, ctx->user_ctx
		))
		{
			return false;
		}
	}

	return true;
}

static bool
lip_iterate_imports(
	lip_function_t* fn,
	lip_import_iteratee_t iteratee,
	void* ctx
)
{
	struct lip_import_itr_ctx_s itr_ctx = {
		.user_ctx = ctx,
		.iteratee = iteratee
	};
	return lip_iterate_functions(fn, lip_apply_import_iteratee, &itr_ctx);
}


static bool
lip_replace_import_instructions(
	lip_function_t* fn,
	lip_function_layout_t* layout,
	void* ctx
)
{
	(void)ctx;
	for(uint16_t i = 0; i < fn->num_instructions; ++i)
	{
		lip_instruction_t* instr = &layout->instructions[i];
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(*instr, &opcode, &operand);

		if(opcode == LIP_OP_IMP) { *instr = lip_asm(LIP_OP_IMPS, operand); }
	}

	return true;
}

static bool
lip_hard_link_import(
	lip_function_t* fn,
	lip_import_t* import,
	lip_loc_range_t loc,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name,
	void* ctx_
)
{
	(void)fn;
	(void)loc;

	struct lip_link_ctx_s* link_ctx = ctx_;
	lip_context_t* ctx = link_ctx->ctx;
	khash_t(lip_module)* module = link_ctx->module;

	lip_symbol_t* symbol = NULL;
	if(module_name.length == 0 && module != NULL)
	{
		khiter_t itr =  kh_get(lip_module, module, symbol_name);
		if(itr != kh_end(module)) { symbol = &kh_value(module, itr); }
	}

	if(symbol == NULL)
	{
		symbol = lip_lookup_symbol_in_symtab(
			ctx->runtime->symtab, module_name, symbol_name
		);
	}

	lip_assert(ctx, symbol != NULL);

	import->value = (lip_value_t) {
		.type = LIP_VAL_FUNCTION,
		.data = { .reference = symbol->value }
	};

	return true;
}

static void
lip_hard_link_function(
	lip_context_t* ctx, lip_function_t* fn, khash_t(lip_module)* module
)
{
	lip_assert(ctx, ctx->rt_read_lock_depth + ctx->rt_write_lock_depth > 0);

	struct lip_link_ctx_s link_ctx = {
		.ctx = ctx,
		.module = module,
		.top_level_fn = fn
	};
	lip_iterate_imports(fn, lip_hard_link_import, &link_ctx);
	lip_iterate_functions(fn, lip_replace_import_instructions, &link_ctx);
}

static bool
lip_soft_link_import(
	lip_function_t* fn,
	lip_import_t* import,
	lip_loc_range_t loc,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name,
	void* ctx_
)
{
	lip_context_t* ctx = ctx_;
	lip_string_t* import_name = lip_function_resource(fn, import->name);

	// Try to resolve symbol using pending modules
	khiter_t itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
	if(itr != kh_end(ctx->loading_symtab))
	{
		khash_t(lip_module)* module = kh_val(ctx->loading_symtab, itr);
		khiter_t itr = kh_get(lip_module, module, symbol_name);
		if(itr != kh_end(module) && kh_val(module, itr).is_public)
		{
			return true;
		}
		else
		{
			lip_set_undefined_symbol_error(ctx, fn, import_name, loc, false);
			return false;
		}
	}

	// Try to resolve symbol using loaded modules
	itr = kh_get(lip_symtab, ctx->runtime->symtab, module_name);
	if(itr != kh_end(ctx->runtime->symtab))
	{
		khash_t(lip_module)* module = kh_val(ctx->runtime->symtab, itr);
		khiter_t itr = kh_get(lip_module, module, symbol_name);
		if(itr != kh_end(module) && kh_val(module, itr).is_public)
		{
			return true;
		}
		else
		{
			lip_set_undefined_symbol_error(ctx, fn, import_name, loc, false);
			return false;
		}
	}

	// Try to load the referenced module
	if(!lip_load_module(ctx, module_name))
	{
		lip_set_undefined_symbol_error(ctx, fn, import_name, loc, true);
		return false;
	}

	// Try to resolve symbol using the module just loaded
	itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
	lip_assert(ctx, itr != kh_end(ctx->loading_symtab));
	khash_t(lip_module)* module = kh_val(ctx->loading_symtab, itr);
	itr = kh_get(lip_module, module, symbol_name);
	if(!(itr != kh_end(module) && kh_val(module, itr).is_public))
	{
		lip_set_undefined_symbol_error(ctx, fn, import_name, loc, false);
		return false;
	}

	return true;
}

bool
lip_link_function(lip_context_t* ctx, lip_function_t* fn)
{
	lip_assert(ctx, ctx->load_depth > 0);
	bool linked = lip_iterate_imports(fn, lip_soft_link_import, ctx);

	if(linked)
	{
		int ret;
		kh_put(lip_ptr_set, ctx->new_script_functions, fn, &ret);
	}

	return linked;
}

void
lip_ctx_end_load(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->load_depth > 0);
	if(--ctx->load_depth == 0 && !ctx->load_aborted)
	{
		kh_foreach(itr, ctx->loading_symtab)
		{
			lip_commit_module_locked(
				ctx,
				kh_key(ctx->loading_symtab, itr),
				kh_val(ctx->loading_symtab, itr)
			);
		}

		kh_foreach(itr, ctx->new_exported_functions)
		{
			lip_hard_link_function(
				ctx,
				(void*)kh_key(ctx->new_exported_functions, itr),
				kh_value(ctx->new_exported_functions, itr)
			);
		}

		kh_foreach(itr, ctx->new_script_functions)
		{
			lip_hard_link_function(
				ctx, kh_key(ctx->new_script_functions, itr), NULL
			);
		}
	}

	lip_ctx_end_rt_write(ctx);
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

static bool
lip_link_module_import_pre_exec(
	lip_function_t* fn,
	lip_import_t* import,
	lip_loc_range_t loc,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name,
	void* ctx
)
{
	// All built-ins and local functions will be checked post-exec
	if(module_name.length == 0) { return true; }

	return lip_soft_link_import(fn, import, loc, module_name, symbol_name, ctx);
}

static bool
lip_link_module_pre_exec(lip_context_t* ctx, lip_function_t* fn)
{
	return lip_iterate_imports(fn, lip_link_module_import_pre_exec, ctx);
}

static bool
lip_link_module_import_post_exec(
	lip_function_t* fn,
	lip_import_t* import,
	lip_loc_range_t loc,
	lip_string_ref_t module_name,
	lip_string_ref_t symbol_name,
	void* ctx_
)
{
	struct lip_link_ctx_s* link_ctx = ctx_;
	lip_context_t* ctx = link_ctx->ctx;
	khash_t(lip_module)* module = link_ctx->module;

	if(module_name.length == 0)
	{
		if(kh_get(lip_module, module, symbol_name) != kh_end(module))
		{
			return true;
		}

		if(true
			&& link_ctx->top_level_fn != fn
			&& lip_string_ref_equal(symbol_name, lip_string_ref("declare"))
		)
		{
			lip_set_link_error(
				ctx, lip_string_ref("Cannot use `declare` inside a `declare`-d function"),
				fn, loc, false
			);
			return false;
		}
	}

	return lip_soft_link_import(fn, import, loc, module_name, symbol_name, ctx);
}

static bool
lip_link_module_post_exec(
	lip_context_t* ctx, lip_function_t* fn, khash_t(lip_module)* module
)
{
	struct lip_link_ctx_s link_ctx = {
		.ctx = ctx,
		.module = module,
		.top_level_fn = fn
	};
	return lip_iterate_imports(fn, lip_link_module_import_post_exec, &link_ctx);
}

bool
lip_load_module(lip_context_t* ctx, lip_string_ref_t name)
{
	int ret;
	kh_put(lip_string_ref_set, ctx->loading_modules, name, &ret);
	if(ret == 0)
	{
		lip_array(char) error_msg_buf = lip_array_create(ctx->module_pool, char, 64);
		lip_sprintf(
			&error_msg_buf,
			"Circular dependency around %.*s", (int)name.length, name.ptr
		);
		lip_array_push(error_msg_buf, '\0');

		ctx->error = (lip_context_error_t){
			.message = {
				.length = lip_array_len(error_msg_buf) - 1,
				.ptr = error_msg_buf
			},
		};
		return false;
	}

	bool result = false;
	bool stacked_error = false;
	lip_ctx_begin_load(ctx);
#define returnVal(X) do { result = X; goto end; } while(0)

	unsigned int num_patterns = ctx->runtime->cfg.num_module_search_patterns;
	const lip_string_ref_t* patterns = ctx->runtime->cfg.module_search_patterns;
	lip_fs_t* fs = ctx->runtime->cfg.fs;
	lip_in_t* file = NULL;
	lip_script_t* script = NULL;
	lip_string_ref_t filename;

	lip_array_clear(ctx->error_records);

	// Try all search patterns until the first one that matches an existing file
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
				.length = lip_array_len(error_msg_buf) - 1,
				.ptr = error_msg_buf
			},
			.num_records = lip_array_len(ctx->error_records),
			.records = ctx->error_records
		};
		stacked_error = true;
		returnVal(false);
	}

	script = lip_load_script(ctx, filename, file);
	if(script == NULL) { returnVal(false); }

	// Ignore all local or builtin references and let the dynamic linker deals
	// with them
	if(!lip_link_module_pre_exec(ctx, script->closure->function.lip))
	{
		returnVal(false);
	}

	khash_t(lip_module)* module = kh_init(lip_module, ctx->module_pool);
	lip_string_ref_t name_copy = lip_copy_string_ref(ctx->module_pool, name);
	khiter_t itr = kh_put(lip_symtab, ctx->loading_symtab, name_copy, &ret);
	kh_val(ctx->loading_symtab, itr) = module;

	khash_t(lip_module)* previous_module = ctx->current_module;
	ctx->current_module = module;

	lip_vm_t* vm = lip_get_default_vm(ctx);
	lip_reset_vm(vm);
	lip_value_t exec_result;
	lip_exec_status_t status = lip_call(
		vm,
		&exec_result,
		(lip_value_t){
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = script->closure }
		},
		0
	);

	ctx->current_module = previous_module;

	if(status == LIP_EXEC_OK)
	{
		// Check whether references to local functions can be resolved
		if(!lip_link_module_post_exec(ctx, script->closure->function.lip, module))
		{
			returnVal(false);
		}

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
		lip_traceback(ctx, vm, exec_result);
		lip_array_foreach(lip_error_record_t, record, ctx->error_records)
		{
			record->message = lip_copy_string_ref(ctx->module_pool, record->message);
			record->filename = lip_copy_string_ref(ctx->module_pool, record->filename);
		}
		returnVal(false);
	}

end:
	if(file) { fs->end_read(fs, file); }
	if(script) { lip_unload_script(ctx, script); }

	if(!result)
	{
		if(!stacked_error)
		{
			lip_array(char) error_msg_buf = lip_array_create(ctx->module_pool, char, 64);
			lip_sprintf(
				&error_msg_buf,
				"Could not load module %.*s", (int)name.length, name.ptr
			);
			lip_array_push(error_msg_buf, '\0');

			lip_context_error_t* error_copy =
				lip_new(ctx->module_pool, lip_context_error_t);
			*error_copy = ctx->error;
			ctx->error = (lip_context_error_t){
				.message = {
					.length = lip_array_len(error_msg_buf) - 1,
					.ptr = error_msg_buf
				},
				.parent = error_copy
			};
		}
		lip_ctx_abort_load(ctx);
	}
	lip_ctx_end_load(ctx);
	return result;
}
