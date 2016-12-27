#ifndef LIP_LIP_INTERNAL_H
#define LIP_LIP_INTERNAL_H

#include <lip/lip.h>
#include "platform.h"
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
	lip_allocator_t* allocator;
	khash_t(lip_ptr_set)* contexts;
	khash_t(lip_symtab)* symtab;
	lip_rwlock_t symtab_lock;
};

struct lip_runtime_link_s
{
	lip_runtime_interface_t vtable;
	lip_allocator_t* allocator;
	lip_context_t* ctx;
};

struct lip_context_s
{
	lip_runtime_t* runtime;
	lip_allocator_t* allocator;
	lip_allocator_t* arena_allocator;
	lip_panic_fn_t panic_handler;
	lip_array(lip_error_record_t) error_records;
	lip_context_error_t error;
	lip_parser_t parser;
	lip_compiler_t compiler;
	khash_t(lip_ptr_set)* scripts;
	khash_t(lip_ptr_set)* vms;
};

#endif
