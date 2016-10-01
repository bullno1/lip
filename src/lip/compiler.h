#ifndef LIP_COMPILER_H
#define LIP_COMPILER_H

#include "common.h"
#include "sexp.h"

typedef struct lip_compiler_s lip_compiler_t;

lip_compiler_t*
lip_compiler_create(lip_allocator_t* allocator);

void
lip_compiler_destroy(lip_compiler_t* compiler);

void
lip_compiler_begin(lip_compiler_t* compiler, lip_string_ref_t source_name);

const lip_error_t*
lip_compiler_add_sexp(lip_compiler_t* compiler, const lip_sexp_t* sexp);

lip_function_t*
lip_compiler_end(lip_compiler_t* compiler);

#endif
