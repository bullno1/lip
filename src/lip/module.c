#include "lip_internal.h"
#include <lip/asm.h>
#include "utils.h"

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
	lip_string_ref_t namespace,
	lip_string_ref_t symbol_name
)
{
	khiter_t itr = kh_get(lip_symtab, symtab, namespace);
	if(itr == kh_end(symtab)) { return false; }

	khash_t(lip_ns)* ns = kh_val(symtab, itr);
	itr = kh_get(lip_ns, ns, symbol_name);
	if(itr == kh_end(ns)) { return false; }

	return &kh_val(ns, itr);
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
lip_static_link_function(
	lip_context_t* ctx, lip_function_t* fn, khash_t(lip_ns)* ns
)
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

		lip_string_ref_t module_name, symbol_name;
		lip_split_fqn(symbol_name_ref, &module_name, &symbol_name);

		lip_symbol_t* symbol = NULL;
		if(module_name.length == 0 && ns != NULL)
		{
			khiter_t itr =  kh_get(lip_ns, ns, symbol_name);
			if(itr != kh_end(ns)) { symbol = &kh_value(ns, itr); }
		}
		else
		{
			symbol = lip_lookup_symbol_in_symtab(
				ctx->runtime->symtab, module_name, symbol_name
			);
		}
		lip_assert(ctx, symbol != NULL);
		layout.imports[i].value = (lip_value_t) {
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = symbol->value }
		};
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
			ctx, lip_function_resource(fn, layout.function_offsets[i]), ns
		);
	}
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
			khiter_t itr = kh_put(lip_userdata,
				ctx->new_exported_functions, value.value->function.lip, &ret
			);
			kh_val(ctx->new_exported_functions, itr) = target_ns;
		}

		khiter_t itr = kh_put(lip_ns, target_ns, symbol_name, &ret);
		kh_val(target_ns, itr) = value;
	}
}

static bool
lip_lookup_symbol_locked(
	lip_context_t* ctx,
	lip_string_ref_t namespace,
	lip_string_ref_t symbol_name,
	lip_value_t* result
)
{
	lip_symbol_t* symbol = NULL;

	if(ctx->current_module)
	{
		khiter_t itr = kh_get(lip_ns, ctx->current_module, symbol_name);
		if(itr != kh_end(ctx->current_module))
		{
			symbol = &kh_val(ctx->current_module, itr);
		}
	}

	if(symbol == NULL)
	{
		lip_lookup_symbol_in_symtab(ctx->loading_symtab, namespace, symbol_name);
	}

	if(symbol == NULL)
	{
		symbol = lip_lookup_symbol_in_symtab(
			ctx->runtime->symtab, namespace, symbol_name
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
lip_set_undefined_symbol_error(
	lip_context_t* ctx,
	lip_function_t* fn,
	lip_string_t* name,
	lip_loc_range_t loc,
	bool stack
)
{
	lip_function_layout_t layout;
	lip_function_layout(fn, &layout);

	lip_array(char) msg_buf = lip_array_create(ctx->module_pool, char, 64);
	lip_sprintf(&msg_buf, "Undefined symbol: %.*s", (int)name->length, name->ptr);
	lip_array_push(msg_buf, '\0');
	lip_string_ref_t error_message = {
		.length = lip_array_len(msg_buf) - 1,
		.ptr = msg_buf
	};
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

	kh_val(ns->content, itr) = (lip_symbol_t) {
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
		kh_clear(lip_userdata, ctx->new_exported_functions);
		kh_clear(lip_ptr_set, ctx->new_script_functions);
		ctx->last_vm = NULL;
		ctx->load_aborted = false;
	}
}

void
lip_ctx_end_load(lip_context_t* ctx)
{
	lip_assert(ctx, ctx->load_depth > 0);
	if(--ctx->load_depth == 0 && !ctx->load_aborted)
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
			lip_static_link_function(
				ctx,
				(void*)kh_key(ctx->new_exported_functions, itr),
				kh_value(ctx->new_exported_functions, itr)
			);
		}

		kh_foreach(itr, ctx->new_script_functions)
		{
			lip_static_link_function(
				ctx, kh_key(ctx->new_script_functions, itr), NULL
			);
		}
	}

	lip_ctx_end_rt_write(ctx);
}

void
lip_destroy_all_modules(lip_runtime_t* runtime)
{
	kh_foreach(itr, runtime->symtab)
	{
		lip_free(runtime->cfg.allocator, (void*)kh_key(runtime->symtab, itr).ptr);
		khash_t(lip_ns)* ns = kh_val(runtime->symtab, itr);
		lip_purge_ns(runtime, ns);
		kh_destroy(lip_ns, ns);
	}
}

bool
lip_link_function(
	lip_context_t* ctx, lip_function_t* fn, bool is_module, khash_t(lip_ns)* module
)
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

		if(true
			&& is_module
			&& module_name.length == 0
			&& (!module || kh_get(lip_ns, module, function_name) != kh_end(module))
		)
		{
			continue;
		}

		khiter_t itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
		if(itr != kh_end(ctx->loading_symtab))
		{
			khash_t(lip_ns)* ns = kh_val(ctx->loading_symtab, itr);
			khiter_t itr = kh_get(lip_ns, ns, function_name);
			if(itr != kh_end(ns) && kh_val(ns, itr).is_public)
			{
				continue;
			}
			else
			{
				lip_set_undefined_symbol_error(ctx, fn, name, loc, false);
				return false;
			}
		}

		itr = kh_get(lip_symtab, ctx->runtime->symtab, module_name);
		if(itr != kh_end(ctx->runtime->symtab))
		{
			khash_t(lip_ns)* ns = kh_val(ctx->runtime->symtab, itr);
			khiter_t itr = kh_get(lip_ns, ns, function_name);
			if(itr != kh_end(ns) && kh_val(ns, itr).is_public)
			{
				continue;
			}
			else
			{
				lip_set_undefined_symbol_error(ctx, fn, name, loc, false);
				return false;
			}
		}

		if(!lip_load_module(ctx, module_name))
		{
			lip_set_undefined_symbol_error(ctx, fn, name, loc, true);
			return false;
		}

		itr = kh_get(lip_symtab, ctx->loading_symtab, module_name);
		lip_assert(ctx, itr != kh_end(ctx->loading_symtab));
		khash_t(lip_ns)* ns = kh_val(ctx->loading_symtab, itr);
		itr = kh_get(lip_ns, ns, function_name);
		if(!(itr != kh_end(ns) && kh_val(ns, itr).is_public))
		{
			lip_set_undefined_symbol_error(ctx, fn, name, loc, false);
			return false;
		}
	}

	for(uint16_t i = 0; i < fn->num_functions; ++i)
	{
		bool link_status = lip_link_function(
			ctx,
			lip_function_resource(fn, layout.function_offsets[i]),
			is_module,
			module
		);
		if(!link_status) { return false; }
	}

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

	script = lip_load_script(ctx, filename, file, false);
	if(script == NULL) { returnVal(false); }

	if(!lip_link_function(ctx, script->closure->function.lip, true, NULL))
	{
		returnVal(false);
	}

	khash_t(lip_ns)* module = kh_init(lip_ns, ctx->module_pool);
	lip_string_ref_t name_copy = lip_copy_string_ref(ctx->module_pool, name);
	khiter_t itr = kh_put(lip_symtab, ctx->loading_symtab, name_copy, &ret);
	kh_val(ctx->loading_symtab, itr) = module;

	khash_t(lip_ns)* previous_module = ctx->current_module;
	ctx->current_module = module;

	lip_vm_t* vm = lip_get_default_vm(ctx);
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
		if(!lip_link_function(ctx, script->closure->function.lip, true, module))
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
