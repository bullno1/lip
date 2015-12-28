#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "array.h"
#include "sexp.h"
#include "asm.h"

#define ENSURE(cond, msg) \
	if(!(cond)) { return lip_compile_error(compiler, msg, sexp); }
#define CHECK_COMPILE(status) \
	if((status) != LIP_COMPILE_OK) { return status; }

static const char* main_sym = "main";

void lip_compiler_init(
	lip_compiler_t* compiler,
	lip_allocator_t* allocator,
	lip_write_fn_t error_fn,
	void* error_ctx
)
{
	compiler->allocator = allocator;
	lip_asm_init(&compiler->lasm, allocator);
	lip_bundler_init(&compiler->bundler, allocator);
	compiler->error_fn = error_fn;
	compiler->error_ctx = error_ctx;
}

void lip_compiler_begin(
	lip_compiler_t* compiler,
	lip_compile_mode_t mode
)
{
	lip_asm_begin(&compiler->lasm);
}

static inline lip_compile_status_t lip_compile_error(
	lip_compiler_t* compiler, const char* msg, lip_sexp_t* sexp
)
{
	char buff[2048];
	int num_chars = snprintf(
		buff, 2048,
		"%d:%d - %d:%d: %s\n",
		sexp->start.line, sexp->start.column,
		sexp->end.line, sexp->end.column,
		msg
	);

	if(num_chars > 0)
	{
		compiler->error_fn(buff, num_chars, compiler->error_ctx);
	}

	return LIP_COMPILE_ERROR;
}

static lip_compile_status_t lip_compile_sexp(
	lip_compiler_t* compiler, lip_sexp_t* sexp
);

static inline lip_compile_status_t lip_compile_identifier(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	// TODO: scoping and other kinds of import
	// TODO: pool imports
	lip_asm_index_t symbol =
		lip_asm_new_import(&compiler->lasm, sexp->data.string);
	lip_asm_add(
		&compiler->lasm,
		LIP_OP_LDS, symbol,
		LIP_ASM_END
	);
	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_arguments(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = sexp->data.list;
	size_t arity = lip_array_len(sexp->data.list) - 1;
	for(size_t arg = arity; arg > 0; --arg)
	{
		CHECK_COMPILE(lip_compile_sexp(compiler, &head[arg]));
	}

	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_number(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	// TODO: pool constants
	lip_string_ref_t string = sexp->data.string;
	double number = strtod(string.ptr, NULL);
	lip_value_t value = {
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	};
	lip_asm_add(
		&compiler->lasm,
		LIP_OP_LDC, lip_asm_new_constant(&compiler->lasm, &value),
		LIP_ASM_END
	);
	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_application(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = sexp->data.list;
	size_t arity = lip_array_len(sexp->data.list) - 1;

	if(head->type == LIP_SEXP_SYMBOL)
	{
		if(strcmp(head->data.string.ptr, "if") == 0)
		{
			ENSURE(arity == 2 || arity == 3, "'if' expects 2 or 3 arguments");
			lip_asm_index_t else_label = lip_asm_new_label(&compiler->lasm);
			lip_asm_index_t done_label = lip_asm_new_label(&compiler->lasm);
			CHECK_COMPILE(lip_compile_sexp(compiler, &head[1]));
			lip_asm_add(
				&compiler->lasm,
				LIP_OP_JOF, else_label,
				LIP_ASM_END
			);
			CHECK_COMPILE(lip_compile_sexp(compiler, &head[2]));
			if(arity == 2)
			{
				lip_asm_add(
					&compiler->lasm,
					LIP_OP_JMP, done_label,
					LIP_OP_LABEL, else_label,
					LIP_OP_NIL, 0,
					LIP_OP_LABEL, done_label,
					LIP_ASM_END
				);
			}
			else // arity == 3
			{
				lip_asm_add(
					&compiler->lasm,
					LIP_OP_JMP, done_label,
					LIP_OP_LABEL, else_label,
					LIP_ASM_END
				);
				CHECK_COMPILE(lip_compile_sexp(compiler, &head[3]));
				lip_asm_add(
					&compiler->lasm,
					LIP_OP_LABEL, done_label,
					LIP_ASM_END
				);
			}
		}
		// TODO: compile as regular application:
		// allow identifier resolution as usual but if it resolves to an import
		// then optimize by calling PLUS instead of calling a function
		else if(strcmp(head->data.string.ptr, "+") == 0)
		{
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			lip_asm_add(
				&compiler->lasm,
				LIP_OP_PLUS, arity,
				LIP_ASM_END
			);
		}
		else if(strcmp(head->data.string.ptr, "<") == 0)
		{
			ENSURE(arity == 2, "'<' expects 2 arguments");
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			lip_asm_add(
				&compiler->lasm,
				LIP_OP_LT, lip_array_len(sexp->data.list),
				LIP_ASM_END
			);
		}
		else
		{
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			CHECK_COMPILE(lip_compile_identifier(compiler, head));
			lip_asm_add(
				&compiler->lasm,
				LIP_OP_CALL, arity,
				LIP_ASM_END
			);
		}
	}
	else // list
	{
		CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
		CHECK_COMPILE(lip_compile_application(compiler, head));
		lip_asm_add(
			&compiler->lasm,
			LIP_OP_CALL, arity,
			LIP_ASM_END
		);
	}

	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_list(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	size_t len = lip_array_len(sexp->data.list);
	if(len == 0)
	{
		return lip_compile_error(compiler, "syntax error", sexp);
	}

	lip_sexp_t* head = sexp->data.list;
	if(head->type == LIP_SEXP_LIST || head->type == LIP_SEXP_SYMBOL)
	{
		return lip_compile_application(compiler, sexp);
	}
	else
	{
		return lip_compile_error(compiler, "term cannot be applied", sexp);
	}
}

static inline lip_compile_status_t lip_compile_sexp(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	switch(sexp->type)
	{
		case LIP_SEXP_LIST:
			return lip_compile_list(compiler, sexp);
		case LIP_SEXP_NUMBER:
			return lip_compile_number(compiler, sexp);
		default:
			return lip_compile_identifier(compiler, sexp);
	}
}

lip_compile_status_t lip_compiler_add_sexp(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	return lip_compile_sexp(compiler, sexp);
}

lip_module_t* lip_compiler_end(lip_compiler_t* compiler)
{
	lip_asm_add(&compiler->lasm, LIP_OP_RET, 0, LIP_ASM_END);
	lip_function_t* function = lip_asm_end(&compiler->lasm);
	lip_bundler_begin(&compiler->bundler);
	lip_string_ref_t main_sym_ref = { 4, main_sym };
	lip_bundler_add_lip_function(&compiler->bundler, main_sym_ref, function);
	return lip_bundler_end(&compiler->bundler);
}

void lip_compiler_cleanup(lip_compiler_t* compiler)
{
	lip_bundler_cleanup(&compiler->bundler);
	lip_asm_cleanup(&compiler->lasm);
}
