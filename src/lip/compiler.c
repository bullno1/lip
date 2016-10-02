#include "ex/compiler.h"
#include "utils.h"
#include "ast.h"
#include "array.h"

#define CHECK(cond) do { if(!cond) { return false; } } while(0)
#define LASM(compiler, opcode, operand, location) \
	lip_asm_add(&compiler->current_scope->lasm, opcode, operand, location)

LIP_IMPLEMENT_CONSTRUCTOR_AND_DESTRUCTOR(lip_compiler)

static void
lip_set_error(
	lip_compiler_t* compiler, lip_loc_range_t location, const char* message
)
{
	compiler->error.location = location;
	compiler->error.extra = message;
}

static bool
lip_compile_exp(lip_compiler_t* compiler, const lip_ast_t* ast);

static bool
lip_compile_number(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	double number = ast->data.number;
	int integer = (int)number;
	if(number == (double)integer && -16777216 <= integer && integer <= 16777215)
	{
		LASM(compiler, LIP_OP_LDI, integer, ast->location);
	}
	else
	{
		lip_asm_index_t index = lip_asm_alloc_numeric_constant(
			&compiler->current_scope->lasm, number
		);
		LASM(compiler, LIP_OP_LDC, index, ast->location);
	}

	return true;
}

static bool
lip_compile_string(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_asm_index_t index = lip_asm_alloc_string_constant(
		&compiler->current_scope->lasm, ast->data.string
	);
	LASM(compiler, LIP_OP_LDC, index, ast->location);
	return true;
}

static void
lip_begin_scope(lip_compiler_t* compiler)
{
	lip_scope_t* scope;
	if(compiler->free_scopes)
	{
		scope = compiler->free_scopes;
		compiler->free_scopes = compiler->free_scopes->parent;
		lip_array_clear(scope->var_names);
		lip_array_clear(scope->var_indices);
	}
	else
	{
		scope = lip_new(compiler->allocator, lip_scope_t);
		lip_asm_init(&scope->lasm, compiler->allocator);
		scope->var_names = lip_array_create(
			compiler->allocator, lip_string_ref_t, 1
		);
		scope->var_indices = lip_array_create(
			compiler->allocator, lip_operand_t, 1
		);
	}

	scope->parent = compiler->current_scope;
	compiler->current_scope = scope;
	lip_asm_begin(&scope->lasm, compiler->source_name);
}

static lip_function_t*
lip_end_scope(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	lip_scope_t* scope = compiler->current_scope;

	compiler->current_scope = scope->parent;
	scope->parent = compiler->free_scopes;
	compiler->free_scopes = scope;

	return lip_asm_end(&scope->lasm, allocator);
}


static bool
lip_compile_arguments(lip_compiler_t* compiler, lip_array(lip_ast_t*) args)
{
	size_t arity = lip_array_len(args);
	for(size_t i = 0; i < arity; ++i)
	{
		CHECK(lip_compile_exp(compiler, args[arity - i - 1]));
	}

	return true;
}

static bool
lip_find_local(
	lip_compiler_t* compiler,
	lip_string_ref_t name,
	lip_operand_t* out
)
{
	for(
		lip_scope_t* scope = compiler->current_scope;
		scope != NULL;
		scope = scope->parent
	)
	{
		size_t num_locals = lip_array_len(scope->var_names);
		for(int i = num_locals - 1; i >= 0; --i)
		{
			if(lip_string_ref_equal(name, scope->var_names[i]))
			{
				*out = scope->var_indices[i];
				return true;
			}
		}
	}

	return false;
}

static bool
lip_compile_identifier(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_operand_t index;
	if(lip_find_local(compiler, ast->data.string, &index))
	{
		LASM(compiler, LIP_OP_LDL, index, ast->location);
	}
	else if(lip_string_ref_equal(lip_string_ref("true"), ast->data.string))
	{
		LASM(compiler, LIP_OP_LDB, 1, ast->location);
	}
	else if(lip_string_ref_equal(lip_string_ref("false"), ast->data.string))
	{
		LASM(compiler, LIP_OP_LDB, 0, ast->location);
	}
	else if(lip_string_ref_equal(lip_string_ref("nil"), ast->data.string))
	{
		LASM(compiler, LIP_OP_NIL, 0, ast->location);
	}
	else
	{
		lip_asm_index_t symbol_index = lip_asm_alloc_import(
			&compiler->current_scope->lasm,
			ast->data.string
		);
		LASM(compiler, LIP_OP_LDS, symbol_index, ast->location);
	}

	return true;
}

static bool
lip_compile_application(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	CHECK(lip_compile_arguments(compiler, ast->data.application.arguments));
	CHECK(lip_compile_exp(compiler, ast->data.application.function));
	LASM(
		compiler,
		LIP_OP_CALL, lip_array_len(ast->data.application.arguments),
		ast->location
	);
	return true;
}

static bool
lip_compile_if(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	CHECK(lip_compile_exp(compiler, ast->data.if_.condition));

	lip_scope_t* scope = compiler->current_scope;
	lip_asm_index_t else_label = lip_asm_new_label(&scope->lasm);
	lip_asm_index_t done_label = lip_asm_new_label(&scope->lasm);
	LASM(compiler, LIP_OP_JOF, else_label, LIP_LOC_NOWHERE);
	CHECK(lip_compile_exp(compiler, ast->data.if_.then));
	LASM(compiler, LIP_OP_JMP, done_label, LIP_LOC_NOWHERE);
	LASM(compiler, LIP_OP_LABEL, else_label, LIP_LOC_NOWHERE);
	if(ast->data.if_.else_)
	{
		CHECK(lip_compile_exp(compiler, ast->data.if_.else_));
	}
	else
	{
		LASM(compiler, LIP_OP_NIL, 0, LIP_LOC_NOWHERE);
	}
	LASM(compiler, LIP_OP_LABEL, done_label, LIP_LOC_NOWHERE);

	return true;
}

static bool
lip_compile_block(lip_compiler_t* compiler, lip_array(lip_ast_t*) block)
{
	size_t block_size = lip_array_len(block);

	if(block_size == 0)
	{
		LASM(compiler, LIP_OP_NIL, 0, LIP_LOC_NOWHERE);
		return true;
	}
	else if(block_size == 1)
	{
		return lip_compile_exp(compiler, block[0]);
	}
	else
	{
		for(size_t i = 0; i < block_size - 1; ++i)
		{
			CHECK(lip_compile_exp(compiler, block[i]));
		}
		LASM(compiler, LIP_OP_POP, block_size - 1, LIP_LOC_NOWHERE);
		CHECK(lip_compile_exp(compiler, block[block_size - 1]));
		return true;
	}
}

static lip_asm_index_t
lip_alloc_local(lip_compiler_t* compiler, lip_string_ref_t name)
{
	lip_scope_t* scope = compiler->current_scope;
	lip_array_push(scope->var_names, name);
	size_t num_locals = lip_array_len(scope->var_names);
	if(num_locals <= lip_array_len(scope->var_indices))
	{
		// reuse an index allocated in an earlier sibling 'let'
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

static bool
lip_compile_let(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	size_t num_vars = lip_array_len(compiler->current_scope->var_names);

	// Compile bindings
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		CHECK(lip_compile_exp(compiler, binding->value));
		lip_asm_index_t local = lip_alloc_local(compiler, binding->name);
		LASM(compiler, LIP_OP_SET, local, binding->location);
	}

	// Compile body
	CHECK(lip_compile_block(compiler, ast->data.let.body));

	lip_array_resize(compiler->current_scope->var_names, num_vars);

	return true;
}

static bool
lip_compile_letrec(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	size_t num_vars = lip_array_len(compiler->current_scope->var_names);

	// Compile bindings
	// Declare placeholders
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		lip_asm_index_t local = lip_alloc_local(compiler, binding->name);
		LASM(compiler, LIP_OP_PLHR, local, LIP_LOC_NOWHERE);
	}

	// Bind value to locals
	lip_asm_index_t local_index = num_vars;
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		CHECK(lip_compile_exp(compiler, binding->value));
		LASM(
			compiler,
			LIP_OP_SET, compiler->current_scope->var_indices[local_index++],
			binding->location
		);
	}

	// Make recursive closure
	local_index = num_vars;
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		LASM(
			compiler,
			LIP_OP_RCLS, compiler->current_scope->var_indices[local_index++],
			LIP_LOC_NOWHERE
		);
	}

	// Compile body
	CHECK(lip_compile_block(compiler, ast->data.let.body));

	lip_array_resize(compiler->current_scope->var_names, num_vars);

	return true;
}

static void
lip_set_add(lip_array(lip_string_ref_t)* set, lip_string_ref_t elem)
{
	lip_array_foreach(lip_string_ref_t, str, *set)
	{
		if(lip_string_ref_equal(*str, elem)) { return; }
	}

	lip_array_push(*set, elem);
}

static void
lip_set_remove(lip_array(lip_string_ref_t)* set, lip_string_ref_t elem)
{
	size_t num_elems = lip_array_len(*set);
	for(size_t i = 0; i < num_elems; ++i)
	{
		if(lip_string_ref_equal((*set)[i], elem))
		{
			lip_array_quick_remove(*set, i);
			return;
		}
	}
}

static void
lip_find_free_vars(const lip_ast_t* ast, lip_array(lip_string_ref_t)* out);

static void
lip_find_free_vars_in_block(
	lip_array(lip_ast_t*) block, lip_array(lip_string_ref_t)* out
)
{
	lip_array_foreach(lip_ast_t*, sub_exp, block)
	{
		lip_find_free_vars(*sub_exp, out);
	}
}

static void
lip_find_free_vars(const lip_ast_t* ast, lip_array(lip_string_ref_t)* out)
{
	switch(ast->type)
	{
		case LIP_AST_IDENTIFIER:
			lip_set_add(out, ast->data.string);
			break;
		case LIP_AST_IF:
			lip_find_free_vars(ast->data.if_.condition, out);
			lip_find_free_vars(ast->data.if_.then, out);
			if(ast->data.if_.else_)
			{
				lip_find_free_vars(ast->data.if_.else_, out);
			}
			break;
		case LIP_AST_APPLICATION:
			lip_find_free_vars_in_block(ast->data.application.arguments, out);
			lip_find_free_vars(ast->data.application.function, out);
			break;
		case LIP_AST_LAMBDA:
			lip_find_free_vars_in_block(ast->data.lambda.body, out);
			lip_array_foreach(lip_string_ref_t, param, ast->data.lambda.arguments)
			{
				lip_set_remove(out, *param);
			}
			break;
		case LIP_AST_DO:
			lip_find_free_vars_in_block(ast->data.do_, out);
			break;
		case LIP_AST_LET:
			{
				lip_find_free_vars_in_block(ast->data.let.body, out);
				size_t num_bindings = lip_array_len(ast->data.let.bindings);
				for(int i = num_bindings - 1; i >= 0; --i)
				{
					lip_let_binding_t* binding = &ast->data.let.bindings[i];
					lip_set_remove(out, binding->name);
					lip_find_free_vars(binding->value, out);
				}
			}
			break;
		case LIP_AST_LETREC:
			{
				lip_find_free_vars_in_block(ast->data.let.body, out);
				size_t num_bindings = lip_array_len(ast->data.let.bindings);
				for(int i = num_bindings - 1; i >= 0; --i)
				{
					lip_find_free_vars(ast->data.let.bindings[i].value, out);
				}
				for(int i = num_bindings - 1; i >= 0; --i)
				{
					lip_set_remove(out, ast->data.let.bindings[i].name);
				}
			}
			break;
		case LIP_AST_STRING:
		case LIP_AST_NUMBER:
			break;
	}
}

static bool
lip_compile_lambda(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_begin_scope(compiler);

	// Allocate parameters as local variables
	lip_array_foreach(lip_string_ref_t, arg, ast->data.lambda.arguments)
	{
		lip_alloc_local(compiler, *arg);
	}

	// Allocate free vars
	lip_array_clear(compiler->free_var_names);
	lip_find_free_vars(ast, &compiler->free_var_names);

	lip_array_clear(compiler->free_var_indices);
	lip_scope_t* scope = compiler->current_scope;
	lip_operand_t free_var_index = 0;
	size_t num_free_vars = lip_array_len(compiler->free_var_names);
	size_t num_captures = 0;
	for(size_t i = 0; i < num_free_vars; ++i)
	{
		lip_operand_t local_index;
		if(lip_find_local(compiler, compiler->free_var_names[i], &local_index))
		{
			lip_array_push(scope->var_names, compiler->free_var_names[i]);
			lip_array_push(scope->var_indices, free_var_index--);
			lip_array_push(compiler->free_var_indices, local_index);
			++num_captures;
		}
	}

	// Compile body
	CHECK(lip_compile_block(compiler, ast->data.lambda.body));

	LASM(compiler, LIP_OP_RET, 0, LIP_LOC_NOWHERE);
	lip_function_t* function = lip_end_scope(compiler, compiler->temp_allocator);
	function->num_args = lip_array_len(ast->data.lambda.arguments);

	// Compiler closure capture
	lip_asm_index_t function_index =
		lip_asm_new_function(&compiler->current_scope->lasm, function);
	lip_operand_t operand =
		(function_index & 0xFFF) | ((num_captures & 0xFFF) << 12);

	LASM(compiler, LIP_OP_CLS, operand, ast->location);
	// Pseudo-instructions to capture local variables into closure
	for(size_t i = 0; i < num_captures; ++i)
	{
		LASM(compiler, LIP_OP_LDL, compiler->free_var_indices[i], LIP_LOC_NOWHERE);
	}

	return true;
}

static bool
lip_compile_do(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	return lip_compile_block(compiler, ast->data.do_);
}

static bool
lip_compile_exp(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	switch(ast->type)
	{
		case LIP_AST_NUMBER:
			return lip_compile_number(compiler, ast);
		case LIP_AST_STRING:
			return lip_compile_string(compiler, ast);
		case LIP_AST_IDENTIFIER:
			return lip_compile_identifier(compiler, ast);
		case LIP_AST_APPLICATION:
			return lip_compile_application(compiler, ast);
		case LIP_AST_IF:
			return lip_compile_if(compiler, ast);
		case LIP_AST_LET:
			return lip_compile_let(compiler, ast);
		case LIP_AST_LETREC:
			return lip_compile_letrec(compiler, ast);
		case LIP_AST_LAMBDA:
			return lip_compile_lambda(compiler, ast);
		case LIP_AST_DO:
			return lip_compile_do(compiler, ast);
	}

	lip_set_error(compiler, ast->location, "Unknown error");
	return false;
}

void
lip_compiler_init(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	compiler->allocator = allocator;
	compiler->temp_allocator = lip_temp_allocator_create(allocator, 1024);
	compiler->source_name = lip_string_ref("");
	compiler->current_scope = NULL;
	compiler->free_scopes = NULL;
	compiler->error.code = 0;
	compiler->error.location = LIP_LOC_NOWHERE;
	compiler->free_var_names = lip_array_create(
		compiler->allocator, lip_string_ref_t, 1
	);
	compiler->free_var_indices = lip_array_create(
		compiler->allocator, lip_operand_t, 1
	);
	compiler->error.extra = NULL;
}

static void
lip_compiler_reset(lip_compiler_t* compiler)
{
	while(compiler->current_scope)
	{
		lip_scope_t* scope = compiler->current_scope;
		compiler->current_scope = scope->parent;
		scope->parent = compiler->free_scopes;
		compiler->free_scopes = scope;
	}

	lip_temp_allocator_reset(compiler->temp_allocator);
}

void
lip_compiler_cleanup(lip_compiler_t* compiler)
{
	lip_compiler_reset(compiler);

	for(
		lip_scope_t* scope = compiler->free_scopes;
		scope != NULL;
	)
	{
		lip_scope_t* next_scope = scope->parent;

		lip_asm_cleanup(&scope->lasm);
		lip_array_destroy(scope->var_indices);
		lip_array_destroy(scope->var_names);
		lip_free(compiler->allocator, scope);

		scope = next_scope;
	}

	lip_array_destroy(compiler->free_var_indices);
	lip_array_destroy(compiler->free_var_names);
	lip_temp_allocator_destroy(compiler->temp_allocator);
}

void
lip_compiler_begin(lip_compiler_t* compiler, lip_string_ref_t source_name)
{
	compiler->source_name = source_name;
	lip_compiler_reset(compiler);
	lip_begin_scope(compiler);
	// Push nil so that the next expression has something to pop
	// It can also help to compile an empty file safely and return nil
	LASM(compiler, LIP_OP_NIL, 0, LIP_LOC_NOWHERE);
}

const lip_error_t*
lip_compiler_add_sexp(lip_compiler_t* compiler, const lip_sexp_t* sexp)
{
	lip_result_t result = lip_translate_sexp(compiler->temp_allocator, sexp);
	if(!result.success) { return result.value; }

	LASM(compiler, LIP_OP_POP, 1, LIP_LOC_NOWHERE); // previous exp's result
	if(!lip_compile_exp(compiler, result.value)) { return &compiler->error; }

	return NULL;
}

lip_function_t*
lip_compiler_end(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	LASM(compiler, LIP_OP_RET, 0, LIP_LOC_NOWHERE);
	return lip_end_scope(compiler, allocator);
}
