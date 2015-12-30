#ifndef LIP_COMPILER_H
#define LIP_COMPILER_H

#include "types.h"
#include "enum.h"
#include "asm.h"
#include "bundler.h"

typedef struct lip_sexp_t lip_sexp_t;
typedef struct lip_module_t lip_module_t;

#define LIP_COMPILE_MODE(F) \
	F(LIP_COMPILE_MODE_REPL) \
	F(LIP_COMPILE_MODE_MODULE)

LIP_ENUM(lip_compile_mode_t, LIP_COMPILE_MODE)

typedef struct lip_scope_t lip_scope_t;

typedef struct lip_compiler_t
{
	lip_allocator_t* allocator;
	lip_bundler_t bundler;
	lip_compile_mode_t mode;
	lip_scope_t* current_scope;
	lip_scope_t* free_scopes;
	lip_write_fn_t error_fn;
	void* error_ctx;
} lip_compiler_t;

void lip_compiler_init(
	lip_compiler_t* compiler,
	lip_allocator_t* allocator,
	lip_write_fn_t error_fn,
	void* error_ctx
);
void lip_compiler_begin(
	lip_compiler_t* compiler,
	lip_compile_mode_t mode
);
bool lip_compiler_add_sexp(lip_compiler_t* compiler, lip_sexp_t* sexp);
lip_module_t* lip_compiler_end(lip_compiler_t* compiler);
void lip_compiler_cleanup(lip_compiler_t* compiler);

#endif
