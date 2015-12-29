#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "array.h"
#include "allocator.h"
#include "sexp.h"
#include "asm.h"
#include "utils.h"

#define ENSURE(cond, msg) \
	if(!(cond)) { return lip_compile_error(compiler, msg, sexp); }
#define CHECK_COMPILE(status) \
	if((status) != LIP_COMPILE_OK) { return status; }
#define LASM(compiler, ...) \
	lip_asm_add(&compiler->current_scope->lasm, __VA_ARGS__, LIP_ASM_END)

typedef struct lip_constant_t
{
	lip_value_type_t type;
	union
	{
		double number;
		lip_string_ref_t string;
	} data;
} lip_constant_t;

// A function scope
typedef struct lip_scope_t
{
	lip_scope_t* parent;
	lip_asm_t lasm;

	lip_array(lip_string_ref_t) var_names;
	lip_array(lip_asm_index_t) var_indices;

	lip_array(lip_constant_t) constant_values;
	lip_array(lip_asm_index_t) constant_indices;

	lip_array(lip_string_ref_t) import_names;
	lip_array(lip_asm_index_t) import_indices;
} lip_function_scope_t;

lip_scope_t* lip_scope_new(lip_compiler_t* compiler)
{
	lip_scope_t* scope = compiler->free_scopes;

	if(scope)
	{
		compiler->free_scopes = scope->parent;
		return scope;
	}
	else
	{
		lip_scope_t* scope = lip_new(compiler->allocator, lip_scope_t);
		lip_asm_init(&scope->lasm, compiler->allocator);
		scope->var_names = lip_array_new(compiler->allocator);
		scope->var_indices = lip_array_new(compiler->allocator);
		scope->constant_values = lip_array_new(compiler->allocator);
		scope->constant_indices = lip_array_new(compiler->allocator);
		scope->import_names = lip_array_new(compiler->allocator);
		scope->import_indices = lip_array_new(compiler->allocator);
		return scope;
	}
}

void lip_scope_delete(lip_compiler_t* compiler, lip_scope_t* scope)
{
	lip_array_delete(scope->import_indices);
	lip_array_delete(scope->import_names);
	lip_array_delete(scope->constant_indices);
	lip_array_delete(scope->constant_values);
	lip_array_delete(scope->var_indices);
	lip_array_delete(scope->var_names);
	lip_asm_cleanup(&scope->lasm);
	lip_free(compiler->allocator, scope);
}

void lip_scope_begin(lip_compiler_t* compiler)
{
	lip_scope_t* scope = lip_scope_new(compiler);

	lip_asm_begin(&scope->lasm);
	lip_array_resize(scope->var_names, 0);
	lip_array_resize(scope->var_indices, 0);
	lip_array_resize(scope->constant_values, 0);
	lip_array_resize(scope->constant_indices, 0);
	lip_array_resize(scope->import_names, 0);
	lip_array_resize(scope->import_indices, 0);

	scope->parent = compiler->current_scope;
	compiler->current_scope = scope;
}

void lip_scope_end(lip_compiler_t* compiler)
{
	lip_scope_t* current_scope = compiler->current_scope;
	compiler->current_scope = current_scope->parent;

	current_scope->parent = compiler->free_scopes;
	compiler->free_scopes = current_scope;
}

void lip_compiler_init(
	lip_compiler_t* compiler,
	lip_allocator_t* allocator,
	lip_write_fn_t error_fn,
	void* error_ctx
)
{
	compiler->allocator = allocator;
	lip_bundler_init(&compiler->bundler, allocator);
	compiler->current_scope = NULL;
	compiler->free_scopes = NULL;
	compiler->error_fn = error_fn;
	compiler->error_ctx = error_ctx;
}

static inline void lip_compiler_reset(lip_compiler_t* compiler)
{
	while(compiler->current_scope)
	{
		lip_scope_end(compiler);
	}
}

void lip_compiler_begin(
	lip_compiler_t* compiler,
	lip_compile_mode_t mode
)
{
	compiler->mode = mode;
	lip_compiler_reset(compiler);

	if(mode == LIP_COMPILE_MODE_REPL)
	{
		lip_scope_begin(compiler);
		// Push nil so that the next expression has something to pop
		// It can also compile an empty file safely and return nil as a result
		LASM(compiler, LIP_OP_NIL, 0);
	}
	else
	{
		lip_bundler_begin(&compiler->bundler);
	}
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
static lip_compile_status_t lip_compile_list(
	lip_compiler_t* compiler, lip_sexp_t* sexp
);

static inline bool lip_scope_find_local(
	lip_scope_t* scope,
	lip_string_ref_t identifier,
	lip_asm_index_t* result
)
{
	// Search backward to allow var shadowing
	unsigned int num_locals = lip_array_len(scope->var_names);
	for(int i = num_locals - 1; i >= 0; --i)
	{
		if(lip_string_ref_equal(identifier, scope->var_names[i]))
		{
			*result = scope->var_indices[i];
			return true;
		}
	}

	return false;
}

static inline bool lip_find_local(
	lip_compiler_t* compiler,
	lip_string_ref_t identifier,
	lip_asm_index_t* result
)
{
	// Search upward from current scope
	for(
		lip_scope_t* scope = compiler->current_scope;
		scope != NULL;
		scope = scope->parent
	)
	{
		if(lip_scope_find_local(scope, identifier, result))
		{
			return true;
		}
	}

	return false;
}

static inline lip_asm_index_t lip_find_import(
	lip_compiler_t* compiler,
	lip_string_ref_t identifier
)
{
	lip_scope_t* scope = compiler->current_scope;
	unsigned int num_imports = lip_array_len(scope->import_names);
	for(unsigned int i = 0; i < num_imports; ++i)
	{
		if(lip_string_ref_equal(identifier, scope->import_names[i]))
		{
			return scope->import_indices[i];
		}
	}

	lip_array_push(scope->import_names, identifier);
	lip_asm_index_t index = lip_asm_new_import(&scope->lasm, identifier);
	lip_array_push(scope->import_indices, index);
	return index;
}

static inline lip_compile_status_t lip_compile_identifier(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	// TODO: warn or error on imports which are special forms
	lip_string_ref_t identifier = sexp->data.string;

	lip_asm_index_t index;
	if(lip_find_local(compiler, identifier, &index))
	{
		LASM(compiler, LIP_OP_LDL, index);
	}
	else
	{
		LASM(compiler, LIP_OP_LDS, lip_find_import(compiler, identifier));
	}

	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_arguments(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = sexp->data.list;
	unsigned int arity = lip_array_len(sexp->data.list) - 1;
	for(unsigned int arg = arity; arg > 0; --arg)
	{
		CHECK_COMPILE(lip_compile_sexp(compiler, &head[arg]));
	}

	return LIP_COMPILE_OK;
}

static inline lip_asm_index_t lip_find_constant(
	lip_compiler_t* compiler, lip_constant_t* constant
)
{
	lip_scope_t* scope = compiler->current_scope;
	unsigned int num_consts = lip_array_len(scope->constant_values);
	for(unsigned int i = 0; i < num_consts; ++i)
	{
		int cmp = memcmp(
			&scope->constant_values[i], constant, sizeof(lip_constant_t)
		);
		if(cmp == 0)
		{
			return scope->constant_indices[i];
		}
	}

	lip_array_push(scope->constant_values, *constant);
	lip_asm_index_t index;
	switch(constant->type)
	{
		case LIP_VAL_NUMBER:
			index =
				lip_asm_new_number_const(&scope->lasm, constant->data.number);
			break;
		case LIP_VAL_STRING:
			index =
				lip_asm_new_string_const(&scope->lasm, constant->data.string);
			break;
		default:
			abort();
	}
	lip_array_push(scope->constant_indices, index);
	return index;
}

static inline lip_compile_status_t lip_compile_constant(
	lip_compiler_t* compiler, lip_constant_t* constant
)
{
	LASM(compiler, LIP_OP_LDC, lip_find_constant(compiler, constant));
	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_string(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_constant_t constant = {
		.type = LIP_VAL_STRING,
		.data = { .string = sexp->data.string }
	};
	return lip_compile_constant(compiler, &constant);
}

static inline lip_compile_status_t lip_compile_number(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_string_ref_t string = sexp->data.string;
	double number = strtod(string.ptr, NULL);
	lip_constant_t constant = {
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	};
	return lip_compile_constant(compiler, &constant);
}

static inline lip_compile_status_t lip_compile_application(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = sexp->data.list;
	unsigned int arity = lip_array_len(sexp->data.list) - 1;

	lip_scope_t* scope = compiler->current_scope;
	if(head->type == LIP_SEXP_SYMBOL)
	{
		if(strcmp(head->data.string.ptr, "if") == 0)
		{
			ENSURE(arity == 2 || arity == 3, "'if' expects 2 or 3 arguments");
			lip_asm_index_t else_label = lip_asm_new_label(&scope->lasm);
			lip_asm_index_t done_label = lip_asm_new_label(&scope->lasm);
			CHECK_COMPILE(lip_compile_sexp(compiler, &head[1]));
			LASM(compiler, LIP_OP_JOF, else_label);
			CHECK_COMPILE(lip_compile_sexp(compiler, &head[2]));
			if(arity == 2)
			{
				LASM(compiler,
					LIP_OP_JMP, done_label,
					LIP_OP_LABEL, else_label,
					LIP_OP_NIL, 0,
					LIP_OP_LABEL, done_label
				);
			}
			else // arity == 3
			{
				LASM(compiler,
					LIP_OP_JMP, done_label,
					LIP_OP_LABEL, else_label
				);
				CHECK_COMPILE(lip_compile_sexp(compiler, &head[3]));
				LASM(compiler, LIP_OP_LABEL, done_label);
			}
		}
		// TODO: optimize in asm instead, search for import->call sequence and
		// change to instruction before label collection
		else if(strcmp(head->data.string.ptr, "+") == 0)
		{
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			LASM(compiler, LIP_OP_PLUS, arity);
		}
		else if(strcmp(head->data.string.ptr, "<") == 0)
		{
			ENSURE(arity == 2, "'<' expects 2 arguments");
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			LASM(compiler, LIP_OP_LT, 0);
		}
		else
		{
			CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
			CHECK_COMPILE(lip_compile_identifier(compiler, head));
			LASM(compiler, LIP_OP_CALL, arity);
		}
	}
	else // list
	{
		CHECK_COMPILE(lip_compile_arguments(compiler, sexp));
		CHECK_COMPILE(lip_compile_list(compiler, head));
		LASM(compiler, LIP_OP_CALL, arity);
	}

	return LIP_COMPILE_OK;
}

static inline lip_compile_status_t lip_compile_list(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	unsigned int len = lip_array_len(sexp->data.list);
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
		case LIP_SEXP_SYMBOL:
			return lip_compile_identifier(compiler, sexp);
		case LIP_SEXP_STRING:
			return lip_compile_string(compiler, sexp);
		case LIP_SEXP_NUMBER:
			return lip_compile_number(compiler, sexp);
	}

	return lip_compile_error(compiler, "not implemented", sexp);
}

lip_compile_status_t lip_compiler_add_sexp(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	if(compiler->mode == LIP_COMPILE_MODE_REPL)
	{
		LASM(compiler, LIP_OP_POP, 1);
		return lip_compile_sexp(compiler, sexp);
	}
	else
	{
		return LIP_COMPILE_ERROR;
	}
}

lip_module_t* lip_compiler_end(lip_compiler_t* compiler)
{
	if(compiler->mode == LIP_COMPILE_MODE_REPL)
	{
		LASM(compiler, LIP_OP_RET, 0);
		lip_function_t* function = lip_asm_end(&compiler->current_scope->lasm);
		lip_scope_end(compiler);
		lip_bundler_begin(&compiler->bundler);
		lip_bundler_add_lip_function(
			&compiler->bundler, lip_string_ref("main"), function
		);
		return lip_bundler_end(&compiler->bundler);
	}
	else
	{
		return NULL;
	}
}

void lip_compiler_cleanup(lip_compiler_t* compiler)
{
	lip_compiler_reset(compiler);
	lip_scope_t* itr = compiler->free_scopes;
	while(itr)
	{
		lip_scope_t* next = itr->parent;
		lip_scope_delete(compiler, itr);
		itr = next;
	}

	lip_bundler_cleanup(&compiler->bundler);
}
