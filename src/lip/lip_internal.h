#ifndef LIP_LIP_INTERNAL_H
#define LIP_LIP_INTERNAL_H

#include "platform.h"
#include <lip/lip.h>
#include <lip/vendor/khash.h>
#include <lip/vm.h>
#include <lip/compiler.h>
#include <lip/parser.h>
#include <lip/array.h>
#include "arena_allocator.h"

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

typedef struct lip_runtime_link_s lip_runtime_link_t;
typedef struct lip_symbol_s lip_symbol_t;

KHASH_DECLARE(lip_module, lip_string_ref_t, lip_symbol_t)
KHASH_DECLARE(lip_symtab, lip_string_ref_t, khash_t(lip_module)*)
KHASH_DECLARE(lip_ptr_set, void*, char)
KHASH_DECLARE(lip_userdata, const void*, void*)

struct lip_symbol_s
{
	bool is_public;
	lip_closure_t* value;
};

struct lip_module_context_s
{
	lip_allocator_t* allocator;
	lip_string_ref_t name;
	khash_t(lip_module)* content;
};

struct lip_runtime_s
{
	lip_runtime_config_t cfg;
	khash_t(lip_ptr_set)* contexts;
	khash_t(lip_symtab)* symtab;
	khash_t(lip_userdata)* userdata;
	lip_rwlock_t rt_lock;
	bool own_fs;
};

struct lip_runtime_link_s
{
	lip_runtime_interface_t vtable;
	lip_allocator_t* allocator;
	lip_context_t* ctx;
	khash_t(lip_userdata)* userdata;
};

struct lip_context_s
{
	lip_runtime_t* runtime;
	lip_allocator_t* allocator;
	lip_allocator_t* temp_pool;
	lip_allocator_t* module_pool;
	lip_panic_fn_t panic_handler;
	lip_array(lip_error_record_t) error_records;
	lip_context_error_t error;
	lip_parser_t parser;
	lip_compiler_t compiler;
	khash_t(lip_ptr_set)* scripts;
	khash_t(lip_ptr_set)* vms;
	lip_array(char) string_buff;
	lip_vm_t* default_vm;
	khash_t(lip_userdata)* userdata;
	khash_t(lip_symtab)* loading_symtab;
	khash_t(lip_string_ref_set)* loading_modules;
	khash_t(lip_userdata)* new_exported_functions;
	khash_t(lip_ptr_set)* new_script_functions;
	khash_t(lip_module)* current_module;
	lip_vm_t* last_vm;
	lip_value_t last_result;
	bool load_aborted;
	unsigned int load_depth;
	unsigned int rt_read_lock_depth;
	unsigned int rt_write_lock_depth;
};

struct lip_script_s
{
	lip_closure_t* closure;
	bool linked;
};

void
lip_ctx_begin_load(lip_context_t* ctx);

void
lip_ctx_end_load(lip_context_t* ctx);

bool
lip_link_function(lip_context_t* ctx, lip_function_t* fn);

void
lip_ctx_begin_rt_read(lip_context_t* ctx);

void
lip_ctx_end_rt_read(lip_context_t* ctx);

void
lip_ctx_begin_rt_write(lip_context_t* ctx);

void
lip_ctx_end_rt_write(lip_context_t* ctx);

lip_string_ref_t
lip_format_parse_error(const lip_error_t* error);

void
lip_unload_all_scripts(lip_context_t* ctx);

void
lip_destroy_all_modules(lip_runtime_t* runtime);

LIP_MAYBE_UNUSED static inline void
lip_set_context_error(
	lip_context_t* ctx,
	const char* error_type,
	lip_string_ref_t message,
	lip_string_ref_t filename,
	lip_loc_range_t location
)
{
	lip_array_clear(ctx->error_records);
	*lip_array_alloc(ctx->error_records) = (lip_error_record_t) {
		.filename = filename,
		.location = location,
		.message = message
	};
	ctx->error = (lip_context_error_t) {
		.message = lip_string_ref(error_type),
		.num_records = 1,
		.records = ctx->error_records
	};
}

LIP_MAYBE_UNUSED static inline void
lip_set_compile_error(
	lip_context_t* ctx,
	lip_string_ref_t message,
	lip_string_ref_t filename,
	lip_loc_range_t location
)
{
	lip_set_context_error(ctx, "Syntax error", message, filename, location);
}

LIP_MAYBE_UNUSED static inline void
lip_panic(lip_context_t* ctx, const char* msg)
{
	ctx->panic_handler(ctx, msg);
}

LIP_MAYBE_UNUSED static lip_closure_t*
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

#endif
