#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "array.h"
#include "allocator.h"
#include "sexp.h"
#include "asm.h"
#include "utils.h"
#include "enum.h"

#define ENSURE(cond, msg) \
	if(!(cond)) { return lip_compile_error(compiler, msg, sexp); }
#define CHECK(status) if(!(status)) { return false; }
#define LASM(compiler, ...) \
	lip_asm_add(&compiler->current_scope->lasm, __VA_ARGS__, LIP_ASM_END)

#define LIP_AST(F) \
	F(LIP_AST_NUMBER) \
	F(LIP_AST_STRING) \
	F(LIP_AST_IDENTIFIER) \
	F(LIP_AST_APPLICATION) \
	F(LIP_AST_IF) \
	F(LIP_AST_LET) \
	F(LIP_AST_LAMBDA) \
	F(LIP_AST_ERROR)

LIP_ENUM(lip_ast_type_t, LIP_AST)

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
	lip_array_clear(scope->var_names);
	lip_array_clear(scope->var_indices);
	lip_array_clear(scope->constant_values);
	lip_array_clear(scope->constant_indices);
	lip_array_clear(scope->import_names);
	lip_array_clear(scope->import_indices);

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

static inline lip_ast_type_t lip_ast_type(lip_sexp_t* sexp)
{
	lip_sexp_t* list;
	switch(sexp->type)
	{
		case LIP_SEXP_LIST:
			list = sexp->data.list;

			if(lip_array_len(list) == 0)
			{
				return LIP_AST_ERROR;
			}
			else if(list[0].type == LIP_SEXP_SYMBOL)
			{
				lip_string_ref_t symbol = list[0].data.string;
				if(lip_string_ref_equal(symbol, lip_string_ref("if")))
				{
					return LIP_AST_IF;
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("let")))
				{
					return LIP_AST_LET;
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("lambda")))
				{
					return LIP_AST_LAMBDA;
				}
				else
				{
					return LIP_AST_APPLICATION;
				}
			}
			else
			{
				return LIP_AST_APPLICATION;
			}
		case LIP_SEXP_SYMBOL:
			return LIP_AST_IDENTIFIER;
		case LIP_SEXP_STRING:
			return LIP_AST_STRING;
		case LIP_SEXP_NUMBER:
			return LIP_AST_NUMBER;
	}

	return LIP_AST_ERROR;
}

static inline bool lip_compile_error(
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

	return false;
}

static inline bool lip_check_syntax(lip_compiler_t* compiler, lip_sexp_t* sexp);

static inline bool lip_check_if_syntax(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity == 2 || arity == 3,
		"'if' must have the form: (if <cond> <then> [else])"
	);

	CHECK(lip_check_syntax(compiler, &list[1]));
	CHECK(lip_check_syntax(compiler, &list[2]));
	if(arity == 3)
	{
		CHECK(lip_check_syntax(compiler, &list[3]));
	}

	return true;
}

static inline bool lip_check_let_syntax(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity == 2 && list[1].type == LIP_SEXP_LIST,
		"'let' must have the form: (let (<bindings...>) <exp>)"
	);

	lip_array_foreach(lip_sexp_t, binding, list[1].data.list)
	{
		bool valid = binding->type == LIP_SEXP_LIST
				  && lip_array_len(binding->data.list) == 2
				  && binding->data.list[0].type == LIP_SEXP_SYMBOL;
		if(!valid)
		{
			return lip_compile_error(
				compiler,
				"a binding must have the form: (<symbol> <exp>)",
				binding
			);
		}
		CHECK(lip_check_syntax(compiler, &binding->data.list[1]));
	}

	CHECK(lip_check_syntax(compiler, &list[2]));

	return true;
}

static inline bool lip_check_lambda_syntax(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity == 2 && list[1].type == LIP_SEXP_LIST,
		"'lambda' must have the form: (lambda (<parameters>) <body>)"
	);

	lip_array_foreach(lip_sexp_t, param, list[1].data.list)
	{
		if(param->type != LIP_SEXP_SYMBOL)
		{
			return lip_compile_error(
				compiler,
				"parameter must be a symbol",
				param
			);
		}
	}

	CHECK(lip_check_syntax(compiler, &list[2]));

	return true;
}

static inline bool lip_check_application_syntax(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = &sexp->data.list[0];

	if(head->type != LIP_SEXP_SYMBOL && head->type != LIP_SEXP_LIST)
	{
		return lip_compile_error(compiler, "term cannot be applied", head);
	}

	lip_array_foreach(lip_sexp_t, sub_exp, sexp->data.list)
	{
		CHECK(lip_check_syntax(compiler, sub_exp));
	}

	return true;
}

static inline bool lip_check_syntax(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	switch(lip_ast_type(sexp))
	{
		case LIP_AST_APPLICATION:
			return lip_check_application_syntax(compiler, sexp);
		case LIP_AST_IF:
			return lip_check_if_syntax(compiler, sexp);
		case LIP_AST_LET:
			return lip_check_let_syntax(compiler, sexp);
		case LIP_AST_LAMBDA:
			return lip_check_lambda_syntax(compiler, sexp);
		case LIP_AST_IDENTIFIER:
		case LIP_AST_STRING:
		case LIP_AST_NUMBER:
			return true;
		case LIP_AST_ERROR:
			return lip_compile_error(compiler, "unrecognized form", sexp);
	}

	return lip_compile_error(compiler, "unrecognized form", sexp);
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
	compiler->free_var_names = lip_array_new(allocator);
	compiler->free_var_indices = lip_array_new(allocator);
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

static bool lip_compile_sexp(
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

static inline bool lip_compile_identifier(
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

	return true;
}

static inline bool lip_compile_arguments(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = sexp->data.list;
	unsigned int arity = lip_array_len(sexp->data.list) - 1;
	for(unsigned int arg = arity; arg > 0; --arg)
	{
		CHECK(lip_compile_sexp(compiler, &head[arg]));
	}

	return true;
}

static inline bool lip_constant_equal(lip_constant_t* lhs, lip_constant_t* rhs)
{
	if(lhs->type != rhs->type) { return false; }

	switch(lhs->type)
	{
		case LIP_VAL_NUMBER:
			return lhs->data.number == rhs->data.number;
		case LIP_VAL_STRING:
			return lip_string_ref_equal(lhs->data.string, rhs->data.string);
		default:
			abort();
			return false;
	}
}

static inline lip_asm_index_t lip_find_constant(
	lip_compiler_t* compiler, lip_constant_t* constant
)
{
	lip_scope_t* scope = compiler->current_scope;
	unsigned int num_consts = lip_array_len(scope->constant_values);
	for(unsigned int i = 0; i < num_consts; ++i)
	{
		if(lip_constant_equal(&scope->constant_values[i], constant))
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

static inline bool lip_compile_constant(
	lip_compiler_t* compiler, lip_constant_t* constant
)
{
	LASM(compiler, LIP_OP_LDC, lip_find_constant(compiler, constant));
	return true;
}

static inline bool lip_compile_string(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	// TODO: decode escape sequences
	lip_constant_t constant = {
		.type = LIP_VAL_STRING,
		.data = { .string = sexp->data.string }
	};
	return lip_compile_constant(compiler, &constant);
}

static inline bool lip_compile_number(
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

static inline lip_asm_index_t lip_new_local(
	lip_compiler_t* compiler, lip_string_ref_t name
)
{
	lip_scope_t* scope = compiler->current_scope;
	lip_array_push(scope->var_names, name);
	unsigned int num_locals = lip_array_len(scope->var_names);
	if(num_locals <= lip_array_len(scope->var_indices))
	{
		// reuse index allocated in an earlier sibling let
		return scope->var_indices[num_locals - 1];
	}
	else
	{
		// allocate a new local
		lip_asm_index_t index = lip_asm_new_local(&scope->lasm);
		lip_array_push(scope->var_indices, index);
		return index;
	}
}

static inline bool lip_compile_if(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_scope_t* scope = compiler->current_scope;
	lip_array(lip_sexp_t) list = sexp->data.list;

	lip_asm_index_t else_label = lip_asm_new_label(&scope->lasm);
	lip_asm_index_t done_label = lip_asm_new_label(&scope->lasm);
	CHECK(lip_compile_sexp(compiler, &list[1]));
	LASM(compiler, LIP_OP_JOF, else_label);
	CHECK(lip_compile_sexp(compiler, &list[2]));
	if(lip_array_len(list) == 3)
	{
		LASM(compiler,
			LIP_OP_JMP, done_label,
			LIP_OP_LABEL, else_label,
			LIP_OP_NIL, 0,
			LIP_OP_LABEL, done_label
		);
	}
	else
	{
		LASM(compiler,
			LIP_OP_JMP, done_label,
			LIP_OP_LABEL, else_label
		);
		CHECK(lip_compile_sexp(compiler, &list[3]));
		LASM(compiler, LIP_OP_LABEL, done_label);
	}

	return true;
}

static inline bool lip_compile_let(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_scope_t* scope = compiler->current_scope;
	unsigned int num_locals = lip_array_len(scope->var_names);

	// Compile bindings
	lip_array_foreach(lip_sexp_t, binding, sexp->data.list[1].data.list)
	{
		CHECK(lip_compile_sexp(compiler, &binding->data.list[1]));
		lip_asm_index_t local = lip_new_local(
			compiler,
			binding->data.list[0].data.string
		);
		LASM(compiler, LIP_OP_SET, local);
	}

	// Compile body
	CHECK(lip_compile_sexp(compiler, &sexp->data.list[2]));

	lip_array_resize(scope->var_names, num_locals);
	return true;
}

void lip_add_var(lip_array(lip_string_ref_t)* free_vars, lip_string_ref_t var)
{
	lip_array_foreach(lip_string_ref_t, str, *free_vars)
	{
		if(lip_string_ref_equal(*str, var)) { return; }
	}

	lip_array_push(*free_vars, var);
}

void lip_remove_var(lip_array(lip_string_ref_t)* free_vars, lip_string_ref_t var)
{
	unsigned int num_vars = lip_array_len(*free_vars);

	for(unsigned int i = 0; i < num_vars; ++i)
	{
		if(lip_string_ref_equal((*free_vars)[i], var))
		{
			(*free_vars)[i] = (*free_vars)[num_vars - 1];
			lip_array_resize(*free_vars, num_vars - 1);
			return;
		}
	}
}

static inline void lip_find_free_vars(
	lip_sexp_t* sexp, lip_array(lip_string_ref_t)* free_vars
)
{
	switch(lip_ast_type(sexp))
	{
		case LIP_AST_IDENTIFIER:
			lip_add_var(free_vars, sexp->data.string);
			break;
		case LIP_AST_IF:
			lip_find_free_vars(&sexp->data.list[1], free_vars);
			lip_find_free_vars(&sexp->data.list[2], free_vars);
			if(lip_array_len(sexp->data.list) == 4)
			{
				lip_find_free_vars(&sexp->data.list[3], free_vars);
			}
			break;
		case LIP_AST_APPLICATION:
			lip_array_foreach(lip_sexp_t, sub_exp, sexp->data.list)
			{
				lip_find_free_vars(sub_exp, free_vars);
			}
			break;
		case LIP_AST_LAMBDA:
			lip_find_free_vars(&sexp->data.list[2], free_vars);
			lip_array_foreach(lip_sexp_t, param, sexp->data.list[1].data.list)
			{
				lip_remove_var(free_vars, param->data.string);
			}
			break;
		case LIP_AST_LET:
			lip_find_free_vars(&sexp->data.list[2], free_vars);
			{
				lip_array(lip_sexp_t) bindings = sexp->data.list[1].data.list;
				int num_bindings = lip_array_len(bindings);
				for(int i = num_bindings - 1; i >= 0; --i)
				{
					lip_sexp_t* binding = &bindings[i];
					lip_remove_var(free_vars, binding->data.list[0].data.string);
					lip_find_free_vars(&binding->data.list[1], free_vars);
				}
			}
			break;
		case LIP_AST_STRING:
		case LIP_AST_NUMBER:
			break;;
		case LIP_AST_ERROR:
			abort();
	}
}

static inline bool lip_compile_lambda(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_scope_begin(compiler);
	// Allocate parameters as local variables
	lip_array_foreach(lip_sexp_t, param, sexp->data.list[1].data.list)
	{
		lip_new_local(compiler, param->data.string);
	}
	// Allocate free vars
	lip_array_clear(compiler->free_var_names);
	lip_array_clear(compiler->free_var_indices);
	lip_find_free_vars(sexp, &compiler->free_var_names);
	unsigned int num_free_vars = lip_array_len(compiler->free_var_names);
	unsigned int num_captures = 0;
	int free_var_index = 0;
	lip_scope_t* scope = compiler->current_scope;
	for(unsigned int i = 0; i < num_free_vars; ++i)
	{
		compiler->free_var_names[num_captures] = compiler->free_var_names[i];

		lip_asm_index_t local_index;
		if(lip_find_local(compiler, compiler->free_var_names[i], &local_index))
		{
			lip_array_push(scope->var_names, compiler->free_var_names[i]);
			lip_array_push(scope->var_indices, free_var_index--);
			lip_array_push(compiler->free_var_indices, local_index);
			++num_captures;
		}
	}
	lip_array_resize(compiler->free_var_names, num_captures);
	// Compile body
	if(!lip_compile_sexp(compiler, &sexp->data.list[2]))
	{
		lip_scope_end(compiler);
		return false;
	}
	LASM(compiler, LIP_OP_RET, 0);
	lip_function_t* function = lip_asm_end(&compiler->current_scope->lasm);
	lip_scope_end(compiler);

	lip_asm_index_t function_index =
		lip_asm_new_function(&compiler->current_scope->lasm, function);
	int32_t operand = (function_index & 0xFFF) | ((num_captures & 0xFFF) << 12);
	LASM(compiler, LIP_OP_CLS, operand);
	// pseudo-instructions to capture local variables into closure
	for(unsigned int i = 0; i < num_captures; ++i)
	{
		LASM(compiler, LIP_OP_LDL, compiler->free_var_indices[i]);
	}
	return true;
}

static inline bool lip_compile_application(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	lip_sexp_t* head = &sexp->data.list[0];
	unsigned int arity = lip_array_len(sexp->data.list) - 1;

	if(head->type == LIP_SEXP_SYMBOL)
	{
		lip_string_ref_t symbol = head->data.string;
		// TODO: optimize in asm instead, search for import->call sequence and
		// change to instruction before label collection
		if(lip_string_ref_equal(symbol, lip_string_ref("+")))
		{
			CHECK(lip_compile_arguments(compiler, sexp));
			LASM(compiler, LIP_OP_PLUS, arity);
		}
		else if(lip_string_ref_equal(symbol, lip_string_ref("<")))
		{
			ENSURE(arity == 2, "'<' expects 2 arguments");
			CHECK(lip_compile_arguments(compiler, sexp));
			LASM(compiler, LIP_OP_LT, 0);
		}
		else
		{
			CHECK(lip_compile_arguments(compiler, sexp));
			CHECK(lip_compile_sexp(compiler, head));
			LASM(compiler, LIP_OP_CALL, arity);
		}
	}
	else
	{
		CHECK(lip_compile_arguments(compiler, sexp));
		CHECK(lip_compile_sexp(compiler, head));
		LASM(compiler, LIP_OP_CALL, arity);
	}

	return true;
}

static inline bool lip_compile_sexp(
	lip_compiler_t* compiler, lip_sexp_t* sexp
)
{
	switch(lip_ast_type(sexp))
	{
		case LIP_AST_IDENTIFIER:
			return lip_compile_identifier(compiler, sexp);
		case LIP_AST_STRING:
			return lip_compile_string(compiler, sexp);
		case LIP_AST_NUMBER:
			return lip_compile_number(compiler, sexp);
		case LIP_AST_IF:
			return lip_compile_if(compiler, sexp);
		case LIP_AST_LET:
			return lip_compile_let(compiler, sexp);
		case LIP_AST_APPLICATION:
			return lip_compile_application(compiler, sexp);
		case LIP_AST_LAMBDA:
			return lip_compile_lambda(compiler, sexp);
		case LIP_AST_ERROR:
			return lip_compile_error(compiler, "unrecognized form", sexp);
	}

	return lip_compile_error(compiler, "not implemented", sexp);
}

bool lip_compiler_add_sexp(lip_compiler_t* compiler, lip_sexp_t* sexp)
{
	CHECK(lip_check_syntax(compiler, sexp));

	if(compiler->mode == LIP_COMPILE_MODE_REPL)
	{
		LASM(compiler, LIP_OP_POP, 1);
		return lip_compile_sexp(compiler, sexp);
	}
	else
	{
		return lip_compile_error(compiler, "not implemented", sexp);
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
		return lip_bundler_end(&compiler->bundler);
	}
}

void lip_compiler_cleanup(lip_compiler_t* compiler)
{
	lip_array_delete(compiler->free_var_names);
	lip_array_delete(compiler->free_var_indices);
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
