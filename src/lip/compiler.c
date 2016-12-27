#include <lip/compiler.h>
#include <assert.h> //TODO: replace with error return
#include <lip/vm.h>
#include <lip/ast.h>
#include <lip/asm.h>
#include <lip/array.h>
#include <lip/arena_allocator.h>

#define LASM(compiler, opcode, operand, location) \
	lip_asm_add(&compiler->current_scope->lasm, opcode, operand, location)

typedef struct lip_var_s lip_var_t;

struct lip_scope_s
{
	lip_scope_t* parent;
	lip_asm_t lasm;

	uint16_t max_num_locals;
	uint16_t current_num_locals;
	lip_array(lip_var_t) vars;
};

struct lip_var_s
{
	lip_string_ref_t name;
	lip_opcode_t load_op;
	lip_asm_index_t index;
};

static void
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
		LASM(compiler, LIP_OP_LDK, index, ast->location);
	}

	return true;
}

static bool
lip_compile_string(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_asm_index_t index = lip_asm_alloc_string_constant(
		&compiler->current_scope->lasm, ast->data.string
	);
	LASM(compiler, LIP_OP_LDK, index, ast->location);
	return true;
}

static lip_scope_t*
lip_begin_scope(lip_compiler_t* compiler)
{
	lip_scope_t* scope;
	if(compiler->free_scopes)
	{
		scope = compiler->free_scopes;
		compiler->free_scopes = compiler->free_scopes->parent;
		lip_array_clear(scope->vars);
	}
	else
	{
		scope = lip_new(compiler->allocator, lip_scope_t);
		lip_asm_init(&scope->lasm, compiler->allocator);
		scope->vars = lip_array_create(
			compiler->allocator, lip_var_t, 1
		);
	}

	scope->parent = compiler->current_scope;
	scope->current_num_locals = 0;
	scope->max_num_locals = 0;
	compiler->current_scope = scope;
	lip_asm_begin(&scope->lasm, compiler->source_name);
	return scope;
}

static lip_function_t*
lip_end_scope(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	lip_scope_t* scope = compiler->current_scope;

	compiler->current_scope = scope->parent;
	scope->parent = compiler->free_scopes;
	compiler->free_scopes = scope;

	lip_function_t* function = lip_asm_end(&scope->lasm, allocator);
	function->num_locals = scope->max_num_locals;
	return function;
}


static void
lip_compile_arguments(lip_compiler_t* compiler, lip_array(lip_ast_t*) args)
{
	size_t arity = lip_array_len(args);
	for(size_t i = 0; i < arity; ++i)
	{
		lip_compile_exp(compiler, args[arity - i - 1]);
	}
}

static bool
lip_find_var(
	lip_scope_t* scope,
	lip_string_ref_t name,
	lip_var_t* out
)
{
	size_t num_locals = lip_array_len(scope->vars);
	for(int i = num_locals - 1; i >= 0; --i)
	{
		if(lip_string_ref_equal(name, scope->vars[i].name))
		{
			*out = scope->vars[i];
			return true;
		}
	}

	return false;
}

static bool
lip_compile_identifier(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_var_t var;
	if(lip_find_var(compiler->current_scope, ast->data.string, &var))
	{
		LASM(compiler, var.load_op, var.index, ast->location);
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
		lip_asm_index_t import_index = lip_asm_alloc_import(
			&compiler->current_scope->lasm,
			ast->data.string
		);
		LASM(compiler, LIP_OP_IMP, import_index, ast->location);
	}

	return true;
}

static bool
lip_compile_application(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_compile_arguments(compiler, ast->data.application.arguments);
	lip_compile_exp(compiler, ast->data.application.function);
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
	lip_compile_exp(compiler, ast->data.if_.condition);

	lip_scope_t* scope = compiler->current_scope;
	lip_asm_index_t else_label = lip_asm_new_label(&scope->lasm);
	lip_asm_index_t done_label = lip_asm_new_label(&scope->lasm);
	LASM(compiler, LIP_OP_JOF, else_label, LIP_LOC_NOWHERE);
	lip_compile_exp(compiler, ast->data.if_.then);
	LASM(compiler, LIP_OP_JMP, done_label, LIP_LOC_NOWHERE);
	LASM(compiler, LIP_OP_LABEL, else_label, LIP_LOC_NOWHERE);
	if(ast->data.if_.else_)
	{
		lip_compile_exp(compiler, ast->data.if_.else_);
	}
	else
	{
		LASM(compiler, LIP_OP_NIL, 0, LIP_LOC_NOWHERE);
	}
	LASM(compiler, LIP_OP_LABEL, done_label, LIP_LOC_NOWHERE);

	return true;
}

static void
lip_compile_block(lip_compiler_t* compiler, lip_array(lip_ast_t*) block)
{
	size_t block_size = lip_array_len(block);

	if(block_size == 0)
	{
		LASM(compiler, LIP_OP_NIL, 0, LIP_LOC_NOWHERE);
	}
	else if(block_size == 1)
	{
		lip_compile_exp(compiler, block[0]);
	}
	else
	{
		for(size_t i = 0; i < block_size - 1; ++i)
		{
			lip_compile_exp(compiler, block[i]);
		}
		LASM(compiler, LIP_OP_POP, block_size - 1, LIP_LOC_NOWHERE);
		lip_compile_exp(compiler, block[block_size - 1]);
	}
}

static lip_asm_index_t
lip_alloc_local(lip_compiler_t* compiler, lip_string_ref_t name)
{
	lip_scope_t* scope = compiler->current_scope;
	uint16_t local_index = scope->current_num_locals++;
	scope->max_num_locals = LIP_MAX(scope->max_num_locals, scope->current_num_locals);
	*lip_array_alloc(scope->vars) = (lip_var_t) {
		.name = name,
		.index = local_index,
		.load_op = LIP_OP_LDLV
	};
	return local_index;
}

static bool
lip_compile_let(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_scope_t* scope = compiler->current_scope;
	size_t num_vars = lip_array_len(scope->vars);
	uint16_t num_locals = scope->current_num_locals;

	// Compile bindings
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		lip_compile_exp(compiler, binding->value);
		lip_asm_index_t local = lip_alloc_local(compiler, binding->name);
		LASM(compiler, LIP_OP_SET, local, binding->location);
	}

	// Compile body
	lip_compile_block(compiler, ast->data.let.body);

	lip_array_resize(scope->vars, num_vars);
	scope->current_num_locals = num_locals;

	return true;
}

static bool
lip_compile_letrec(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_scope_t* scope = compiler->current_scope;
	size_t num_vars = lip_array_len(scope->vars);
	uint16_t num_locals = scope->current_num_locals;

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
		lip_compile_exp(compiler, binding->value);
		LASM(
			compiler,
			LIP_OP_SET, scope->vars[local_index++].index,
			binding->location
		);
	}

	// Make recursive closure
	local_index = num_vars;
	lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
	{
		LASM(
			compiler,
			LIP_OP_RCLS, compiler->current_scope->vars[local_index++].index,
			LIP_LOC_NOWHERE
		);
	}

	// Compile body
	lip_compile_block(compiler, ast->data.let.body);

	lip_array_resize(scope->vars, num_vars);
	scope->current_num_locals = num_locals;

	return true;
}

static void
lip_set_add(khash_t(lip_string_ref_set)* set, lip_string_ref_t elem)
{
	int ret;
	kh_put(lip_string_ref_set, set, elem, &ret);
}

static void
lip_set_remove(khash_t(lip_string_ref_set)* set, lip_string_ref_t elem)
{
	khiter_t iter = kh_get(lip_string_ref_set, set, elem);
	if(iter != kh_end(set)) { kh_del(lip_string_ref_set, set, iter); }
}

static void
lip_find_free_vars(const lip_ast_t* ast, khash_t(lip_string_ref_set)* out);

static void
lip_find_free_vars_in_block(
	lip_array(lip_ast_t*) block, khash_t(lip_string_ref_set)* out
)
{
	lip_array_foreach(lip_ast_t*, sub_exp, block)
	{
		lip_find_free_vars(*sub_exp, out);
	}
}

static void
lip_find_free_vars(const lip_ast_t* ast, khash_t(lip_string_ref_set)* out)
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
	lip_scope_t* scope = lip_begin_scope(compiler);

	// Allocate arguments
	uint16_t arg_index = 0;
	lip_array_foreach(lip_string_ref_t, arg, ast->data.lambda.arguments)
	{
		*lip_array_alloc(scope->vars) = (lip_var_t){
			.name = *arg,
			.index = arg_index++,
			.load_op = LIP_OP_LARG
		};
	}

	// Allocate free vars
	kh_clear(lip_string_ref_set, compiler->free_var_names);
	lip_find_free_vars(ast, compiler->free_var_names);

	lip_var_t* free_vars = lip_malloc(
		compiler->arena_allocator,
		kh_size(compiler->free_var_names) * sizeof(lip_var_t)
	);
	uint8_t captured_var_index = 0;

	kh_foreach(itr, compiler->free_var_names)
	{
		lip_string_ref_t var_name = kh_key(compiler->free_var_names, itr);
		assert(
			lip_find_var(scope->parent, var_name, &free_vars[captured_var_index])
		);// TODO: return error
		*lip_array_alloc(scope->vars) = (lip_var_t){
			.name = var_name,
			.index = captured_var_index++,
			.load_op = LIP_OP_LDCV
		};
	}

	// Compile body
	lip_compile_block(compiler, ast->data.lambda.body);

	LASM(compiler, LIP_OP_RET, 0, LIP_LOC_NOWHERE);
	lip_function_t* function = lip_end_scope(compiler, compiler->arena_allocator);
	function->num_args = lip_array_len(ast->data.lambda.arguments);

	// Compiler closure capture
	lip_asm_index_t function_index =
		lip_asm_new_function(&compiler->current_scope->lasm, function);
	lip_operand_t operand =
		(function_index & 0xFFF) | ((captured_var_index & 0xFFF) << 12);

	LASM(compiler, LIP_OP_CLS, operand, ast->location);
	// Pseudo-instructions to capture local variables into closure
	for(size_t i = 0; i < captured_var_index; ++i)
	{
		LASM(compiler, free_vars[i].load_op, free_vars[i].index, LIP_LOC_NOWHERE);
	}
	lip_free(compiler->arena_allocator, free_vars);

	return true;
}

static void
lip_compile_do(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	lip_compile_block(compiler, ast->data.do_);
}

static void
lip_compile_exp(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	switch(ast->type)
	{
		case LIP_AST_NUMBER:
			lip_compile_number(compiler, ast);
			break;
		case LIP_AST_STRING:
			lip_compile_string(compiler, ast);
			break;
		case LIP_AST_IDENTIFIER:
			lip_compile_identifier(compiler, ast);
			break;
		case LIP_AST_APPLICATION:
			lip_compile_application(compiler, ast);
			break;
		case LIP_AST_IF:
			lip_compile_if(compiler, ast);
			break;
		case LIP_AST_LET:
			lip_compile_let(compiler, ast);
			break;
		case LIP_AST_LETREC:
			lip_compile_letrec(compiler, ast);
			break;
		case LIP_AST_LAMBDA:
			lip_compile_lambda(compiler, ast);
			break;
		case LIP_AST_DO:
			lip_compile_do(compiler, ast);
			break;
	}
}

void
lip_compiler_init(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	compiler->allocator = allocator;
	compiler->arena_allocator = lip_arena_allocator_create(allocator, 1024);
	compiler->source_name = lip_string_ref("");
	compiler->current_scope = NULL;
	compiler->free_scopes = NULL;
	compiler->free_var_names = kh_init(lip_string_ref_set, allocator);
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

	lip_arena_allocator_reset(compiler->arena_allocator);
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
		lip_array_destroy(scope->vars);
		lip_free(compiler->allocator, scope);

		scope = next_scope;
	}

	kh_destroy(lip_string_ref_set, compiler->free_var_names);
	lip_arena_allocator_destroy(compiler->arena_allocator);
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

void
lip_compiler_add_ast(lip_compiler_t* compiler, const lip_ast_t* ast)
{
	LASM(compiler, LIP_OP_POP, 1, LIP_LOC_NOWHERE); // previous exp's result
	lip_compile_exp(compiler, ast);
}

lip_function_t*
lip_compiler_end(lip_compiler_t* compiler, lip_allocator_t* allocator)
{
	LASM(compiler, LIP_OP_RET, 0, LIP_LOC_NOWHERE);
	return lip_end_scope(compiler, allocator);
}
