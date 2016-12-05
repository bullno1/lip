#ifndef LIP_COMPILER_EX_H
#define LIP_COMPILER_EX_H

#include "../compiler.h"
#include "asm.h"
#include "arena_allocator.h"
#include "../vendor/khash.h"

KHASH_DECLARE(lip_string_ref_set, lip_string_ref_t, char)

typedef struct lip_scope_s lip_scope_t;
typedef struct lip_var_s lip_var_t;

struct lip_scope_s
{
	lip_scope_t* parent;
	lip_asm_t lasm;

	lip_array(lip_string_ref_t) var_names;
	lip_array(lip_var_t) var_infos;
};

struct lip_var_s
{
	lip_opcode_t load_op;
	lip_asm_index_t index;
};

struct lip_compiler_s
{
	lip_allocator_t* allocator;
	lip_arena_allocator_t arena_allocator;
	lip_string_ref_t source_name;
	lip_scope_t* current_scope;
	lip_scope_t* free_scopes;
	khash_t(lip_string_ref_set)* free_var_names;
	lip_array(lip_var_t) free_var_infos;
	lip_error_t error;
};

#endif
