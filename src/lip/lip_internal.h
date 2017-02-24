#ifndef LIP_LIP_INTERNAL_H
#define LIP_LIP_INTERNAL_H

#include "platform.h"
#include <lip/lip.h>
#include <lip/vendor/khash.h>
#include <lip/vm.h>
#include <lip/compiler.h>
#include <lip/parser.h>
#include "arena_allocator.h"

typedef struct lip_runtime_link_s lip_runtime_link_t;
typedef struct lip_symbol_s lip_symbol_t;

KHASH_DECLARE(lip_ns, lip_string_ref_t, lip_symbol_t)
KHASH_DECLARE(lip_symtab, lip_string_ref_t, khash_t(lip_ns)*)
KHASH_DECLARE(lip_ptr_set, void*, char)
KHASH_DECLARE(lip_userdata, const void*, void*)

struct lip_symbol_s
{
	lip_closure_t* value;
};

struct lip_ns_context_s
{
	lip_allocator_t* allocator;
	lip_string_ref_t name;
	khash_t(lip_ns)* content;
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
	lip_vm_t* internal_vm;
	khash_t(lip_userdata)* userdata;
	khash_t(lip_symtab)* loading_symtab;
	khash_t(lip_string_ref_set)* loading_modules;
	khash_t(lip_ptr_set)* new_exported_functions;
	khash_t(lip_ptr_set)* new_script_functions;
	unsigned int load_depth;
	unsigned int rt_read_lock_depth;
	unsigned int rt_write_lock_depth;
};

struct lip_script_s
{
	lip_closure_t* closure;
	bool linked;
};

extern const char lip_module_ctx_key;

#endif
