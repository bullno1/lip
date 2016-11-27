#ifndef LIP_COMPILER_EX_H
#define LIP_COMPILER_EX_H

#include "../compiler.h"
#include "asm.h"
#include "arena_allocator.h"
#include "../khash.h"

KHASH_DECLARE(lip_string_ref_set, lip_string_ref_t, char)

typedef struct lip_scope_s lip_scope_t;

struct lip_scope_s
{
	lip_scope_t* parent;
	lip_asm_t lasm;

	lip_array(lip_string_ref_t) var_names;
	lip_array(lip_operand_t) var_indices;
};

struct lip_compiler_s
{
	lip_allocator_t* allocator;
	lip_arena_allocator_t arena_allocator;
	lip_string_ref_t source_name;
	lip_scope_t* current_scope;
	lip_scope_t* free_scopes;
	lip_array(lip_ast_transform_t*) ast_transforms;
	khash_t(lip_string_ref_set)* free_var_names;
	lip_array(lip_operand_t) free_var_indices;
	lip_error_t error;
};

void
lip_compiler_init(lip_compiler_t* compiler, lip_allocator_t* allocator);

void
lip_compiler_cleanup(lip_compiler_t* compiler);

#endif
