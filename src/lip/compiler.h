#ifndef LIP_COMPILER_H
#define LIP_COMPILER_H

#include "ex/common.h"
#include "sexp.h"
#include "ast.h"

typedef struct lip_compiler_s lip_compiler_t;

void
lip_compiler_init(lip_compiler_t* compiler, lip_allocator_t* allocator);

void
lip_compiler_cleanup(lip_compiler_t* compiler);

void
lip_compiler_begin(lip_compiler_t* compiler, lip_string_ref_t source_name);

const lip_error_t*
lip_compiler_add_ast(lip_compiler_t* compiler, const lip_ast_t* ast);

lip_function_t*
lip_compiler_end(lip_compiler_t* compiler, lip_allocator_t* allocator);

#endif
