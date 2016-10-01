#include "ast.h"
#include "memory.h"
#include "stdlib.h"
#include "array.h"

#define CHECK_SEXP(sexp, cond, msg) \
	do { \
		if(!(cond)) { \
			return lip_error(allocator, sexp, msg); \
		} \
	} while(0)

#define ENSURE(cond, msg) CHECK_SEXP(sexp, cond, msg)

#define TRANSLATE(var, sexp) \
	lip_ast_t* var; \
	do { \
		lip_result_t result = lip_translate_sexp(allocator, sexp); \
		if(!result.success) { return result; } \
		var = result.value; \
	} while(0)

#define TRANSLATE_BLOCK(var, sexps, num_sexps) \
	lip_array(lip_ast_t*) var = lip_array_create(\
		allocator, lip_ast_t*, (num_sexps) \
	); \
	for(uint32_t i = 0; i < (num_sexps); ++i) { \
		TRANSLATE(exp, &(sexps)[i]); \
		lip_array_push(var, exp); \
	}

static lip_result_t
lip_success(void* result)
{
	return (lip_result_t){
		.success = true,
		.value = result
	};
}

static lip_result_t
lip_error(lip_allocator_t* allocator, const lip_sexp_t* sexp, const char* msg)
{
	lip_error_t* error = lip_new(allocator, lip_error_t);
	error->code = 0;
	error->extra = msg;
	error->location = sexp->location;
	return (lip_result_t){
		.success = false,
		.value = error
	};
}

static lip_ast_t*
lip_alloc_ast(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* ast = lip_new(allocator, lip_ast_t);
	ast->location = sexp->location;
	return ast;
}

static lip_result_t
lip_translate_if(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity == 2 || arity == 3,
		"'if' must have the form: (if <condition> <then> [else])"
	);
	TRANSLATE(condition, &list[1]);
	TRANSLATE(then, &list[2]);
	lip_ast_t* else_ = NULL;
	if(arity == 3)
	{
		lip_result_t result = lip_translate_sexp(allocator, &list[3]);
		if(!result.success) { return result; }
		else_ = result.value;
	}

	lip_ast_t* if_ = lip_alloc_ast(allocator, sexp);
	if_->type = LIP_AST_IF;
	if_->data.if_.condition = condition;
	if_->data.if_.then = then;
	if_->data.if_.else_ = else_;
	return lip_success(if_);
}

static lip_result_t
lip_translate_let(
	lip_allocator_t* allocator, const lip_sexp_t* sexp, bool recursive
)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity >= 2 && list[1].type == LIP_SEXP_LIST,
		recursive ?
			"'letrec' must have the form: (letrec (<bindings...>) <exp...>)" :
			"'let' must have the form: (let (<bindings...>) <exp...>)"
	);

	lip_array(lip_let_binding_t) bindings = lip_array_create(
		allocator,
		lip_let_binding_t,
		lip_array_len(list[1].data.list)
	);
	lip_array_foreach(lip_sexp_t, binding, list[1].data.list)
	{
		CHECK_SEXP(
			binding,
			binding->type == LIP_SEXP_LIST
			&& lip_array_len(binding->data.list) == 2
			&& binding->data.list[0].type == LIP_SEXP_SYMBOL,
			"a binding must have the form: (<symbol> <expr>)"
		);
		TRANSLATE(value, &binding->data.list[1]);
		lip_array_push(bindings, ((lip_let_binding_t){
			.name = binding->data.list[0].data.string,
			.value = value,
			.location = binding->location
		}));
	}

	TRANSLATE_BLOCK(body, &list[2], arity - 1);

	lip_ast_t* let = lip_alloc_ast(allocator, sexp);
	let->type = recursive ? LIP_AST_LETREC : LIP_AST_LET;
	let->data.let.bindings = bindings;
	let->data.let.body = body;
	return lip_success(let);
}

static lip_result_t
lip_translate_lambda(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity >= 2 && list[1].type == LIP_SEXP_LIST,
		"'fn' must have the form: (fn (<arguments>) <exp...>)"
	);

	lip_array(lip_string_ref_t) arguments = lip_array_create(
		allocator,
		lip_string_ref_t,
		lip_array_len(list[1].data.list)
	);
	lip_array_foreach(lip_sexp_t, arg, list[1].data.list)
	{
		CHECK_SEXP(
			arg, arg->type == LIP_SEXP_SYMBOL, "argument name must be a symbol"
		);
		lip_array_push(arguments, arg->data.string);
	}

	TRANSLATE_BLOCK(body, &list[2], arity - 1);

	lip_ast_t* lambda = lip_alloc_ast(allocator, sexp);
	lambda->type = LIP_AST_LAMBDA;
	lambda->data.lambda.arguments = arguments;
	lambda->data.lambda.body = body;
	return lip_success(lambda);
}

static lip_result_t
lip_translate_do(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	TRANSLATE_BLOCK(
		body, sexp->data.list + 1, lip_array_len(sexp->data.list) - 1
	);

	lip_ast_t* do_ = lip_alloc_ast(allocator, sexp);
	do_->type = LIP_AST_DO;
	do_->data.do_ = body;
	return lip_success(do_);
}

static lip_result_t
lip_translate_application(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_sexp_t* list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	TRANSLATE(function, &list[0]);
	TRANSLATE_BLOCK(arguments, &list[1], arity);

	lip_ast_t* application = lip_alloc_ast(allocator, sexp);
	application->type = LIP_AST_APPLICATION;
	application->data.application.function = function;
	application->data.application.arguments = arguments;
	return lip_success(application);
}

static lip_result_t
lip_translate_identifier(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* identifier = lip_alloc_ast(allocator, sexp);
	identifier->type = LIP_AST_IDENTIFIER;
	identifier->data.string = sexp->data.string;
	return lip_success(identifier);
}

static lip_result_t
lip_translate_string(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* string = lip_alloc_ast(allocator, sexp);
	string->type = LIP_AST_STRING;
	string->data.string = sexp->data.string;
	return lip_success(string);
}

static lip_result_t
lip_translate_number(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* number = lip_alloc_ast(allocator, sexp);
	number->type = LIP_AST_NUMBER;
	// Lexeme was already checked for error before
	number->data.number = strtod(sexp->data.string.ptr, NULL);
	return lip_success(number);
}

lip_result_t
lip_translate_sexp(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	switch(sexp->type)
	{
		case LIP_SEXP_LIST:
			if(lip_array_len(sexp->data.list) == 0)
			{
				return lip_error(allocator, sexp, "Empty list is invalid");
			}
			else if(sexp->data.list[0].type == LIP_SEXP_SYMBOL)
			{
				lip_string_ref_t symbol = sexp->data.list[0].data.string;
				if(lip_string_ref_equal(symbol, lip_string_ref("if")))
				{
					return lip_translate_if(allocator, sexp);
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("let")))
				{
					return lip_translate_let(allocator, sexp, false);
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("letrec")))
				{
					return lip_translate_let(allocator, sexp, true);
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("fn")))
				{
					return lip_translate_lambda(allocator, sexp);
				}
				else if(lip_string_ref_equal(symbol, lip_string_ref("do")))
				{
					return lip_translate_do(allocator, sexp);
				}
				else
				{
					return lip_translate_application(allocator, sexp);
				}
			}
			else
			{
				return lip_translate_application(allocator, sexp);
			}
		case LIP_SEXP_SYMBOL:
			return lip_translate_identifier(allocator, sexp);
		case LIP_SEXP_STRING:
			return lip_translate_string(allocator, sexp);
		case LIP_SEXP_NUMBER:
			return lip_translate_number(allocator, sexp);
	}

	// Impossibru!!
	return lip_error(allocator, sexp, "Unknown error");
}
