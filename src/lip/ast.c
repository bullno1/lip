#include <lip/ast.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <lip/memory.h>
#include <lip/array.h>

#define CHECK_SEXP(sexp, cond, msg) \
	do { \
		if(!(cond)) { \
			return lip_syntax_error(sexp->location, msg); \
		} \
	} while(0)

#define ENSURE(cond, msg) CHECK_SEXP(sexp, cond, msg)

#define TRANSLATE(var, sexp) \
	lip_ast_t* var; \
	do { \
		lip_ast_result_t result = lip_translate_sexp(allocator, sexp); \
		if(!result.success) { return result; } \
		var = result.value.result; \
	} while(0)

#define TRANSLATE_BLOCK(var, sexps, num_sexps) \
	lip_array(lip_ast_t*) var = lip_array_create(\
		allocator, lip_ast_t*, (num_sexps) \
	); \
	for(uint32_t i = 0; i < (num_sexps); ++i) { \
		TRANSLATE(exp, &(sexps)[i]); \
		lip_array_push(var, exp); \
	}

typedef lip_error_m(const lip_sexp_t*) lip_sexp_result_t;

static lip_ast_result_t
lip_success(lip_ast_t* ast)
{
	return (lip_ast_result_t){
		.success = true,
		.value = {.result = ast}
	};
}

static lip_ast_result_t
lip_syntax_error(lip_loc_range_t location, const char* msg)
{
	return (lip_ast_result_t){
		.success = false,
		.value = {
			.error = {
				.location = location,
				.extra = msg
			}
		}
	};
}

static lip_sexp_result_t
lip_quote_success(const lip_sexp_t* sexp)
{
	return (lip_sexp_result_t){
		.success = true,
		.value = {.result = sexp}
	};
}

static lip_sexp_result_t
lip_quote_error(lip_loc_range_t location, const char* msg)
{
	return (lip_sexp_result_t){
		.success = false,
		.value = {
			.error = {
				.location = location,
				.extra = msg
			}
		}
	};
}

static lip_ast_t*
lip_alloc_ast(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* ast = lip_new(allocator, lip_ast_t);
	ast->location = sexp->location;
	return ast;
}

static lip_ast_result_t
lip_translate_if(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;
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
		lip_ast_result_t result = lip_translate_sexp(allocator, &list[3]);
		if(!result.success) { return result; }
		else_ = result.value.result;
	}

	lip_ast_t* if_ = lip_alloc_ast(allocator, sexp);
	if_->type = LIP_AST_IF;
	if_->data.if_.condition = condition;
	if_->data.if_.then = then;
	if_->data.if_.else_ = else_;
	return lip_success(if_);
}

static lip_ast_result_t
lip_translate_let(
	lip_allocator_t* allocator, const lip_sexp_t* sexp, bool recursive
)
{
	lip_array(lip_sexp_t) list = sexp->data.list;
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

static lip_ast_result_t
lip_translate_lambda(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;
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

	size_t pos = 0;
	bool is_vararg = false;
	lip_array_foreach(lip_sexp_t, arg, list[1].data.list)
	{
		CHECK_SEXP(
			arg, arg->type == LIP_SEXP_SYMBOL, "argument name must be a symbol"
		);

		lip_string_ref_t arg_name = arg->data.string;

		++pos;
		if(arg_name.length >= 1 && arg_name.ptr[0] == '&')
		{
			if(pos != lip_array_len(list[1].data.list))
			{
				return lip_syntax_error(
					arg->location, "Only last argument can be prefixed with '&'"
				);
			}

			arg_name.ptr += 1;
			arg_name.length -= 1;
			is_vararg = true;
		}

		lip_array_foreach(lip_string_ref_t, previous_arg, arguments)
		{
			if(lip_string_ref_equal(*previous_arg, arg_name))
			{
				return lip_syntax_error(arg->location, "Duplicated argument name");
			}
		}

		lip_array_push(arguments, arg_name);
	}

	TRANSLATE_BLOCK(body, &list[2], arity - 1);

	lip_ast_t* lambda = lip_alloc_ast(allocator, sexp);
	lambda->type = LIP_AST_LAMBDA;
	lambda->data.lambda.arguments = arguments;
	lambda->data.lambda.body = body;
	lambda->data.lambda.is_vararg = is_vararg;
	return lip_success(lambda);
}

static lip_ast_result_t
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

static lip_ast_result_t
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

static lip_ast_result_t
lip_translate_identifier(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* identifier = lip_alloc_ast(allocator, sexp);
	identifier->type = LIP_AST_IDENTIFIER;
	identifier->data.string = sexp->data.string;
	return lip_success(identifier);
}

static lip_ast_result_t
lip_translate_symbol(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* symbol = lip_alloc_ast(allocator, sexp);
	symbol->type = LIP_AST_SYMBOL;
	symbol->data.string = sexp->data.string;
	return lip_success(symbol);
}

// Reference: http://en.cppreference.com/w/cpp/language/escape
static lip_ast_result_t
lip_translate_string(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* string = lip_alloc_ast(allocator, sexp);
	string->type = LIP_AST_STRING;
	lip_string_ref_t str = sexp->data.string;
	char* unescaped = lip_malloc(allocator, str.length + 1);
	char* outc = unescaped;
	const char* end = str.ptr + str.length;
	for(const char* ch = str.ptr; ch < end; ++ch)
	{
		if(*ch == '\\')
		{
			lip_loc_t start_loc = {
				.line = sexp->location.start.line,
				.column = sexp->location.start.column + (ch - str.ptr) + 1
			};
			++ch;

			switch(*ch)
			{
				case 'a': *outc++ = '\a'; break;
				case 'b': *outc++ = '\b'; break;
				case 'f': *outc++ = '\f'; break;
				case 'n': *outc++ = '\n'; break;
				case 'r': *outc++ = '\r'; break;
				case 't': *outc++ = '\t'; break;
				case 'v': *outc++ = '\v'; break;
				case 'x':
					{
						char hex[3] = { 0 };
						bool has_non_hex_char = false;
						for(unsigned int i = 0; i < 2; ++i)
						{
							++ch;
							if(ch >= end) { break; }
							if(isxdigit(*ch))
							{
								hex[i] = *ch;
							}
							else
							{
								has_non_hex_char = true;
								break;
							}
						}

						unsigned int num;
						if(sscanf(hex, "%x", &num) != 1)
						{
							lip_loc_t end_loc = {
								.line = sexp->location.start.line,
								.column =
									sexp->location.start.column + (ch - str.ptr)
							};
							return lip_syntax_error(
								(lip_loc_range_t){
									.start = start_loc, .end = end_loc
								},
								"Invalid hex escape sequence"
							);
						}
						else
						{
							*outc++ = (char)num;
							if(has_non_hex_char) { *outc++ = *ch; }
						}
					}
					break;
				default:
					if(isdigit(*ch))
					{
						char oct[4] = { [0] = *ch };
						bool has_non_oct_char = false;
						for(unsigned int i = 1; i < 3; ++i)
						{
							++ch;
							if(ch >= end) { break; }
							if('0' <= *ch && *ch <= '7')
							{
								oct[i] = *ch;
							}
							else
							{
								has_non_oct_char = true;
								break;
							}
						}

						unsigned int num;
						if(sscanf(oct, "%o", &num) != 1 || num > UCHAR_MAX)
						{
							lip_loc_t end_loc = {
								.line = sexp->location.start.line,
								.column =
									sexp->location.start.column
									+ (ch - str.ptr) + 1
									+ (has_non_oct_char || ch >= end ? -1 : 0)
							};
							return lip_syntax_error(
								(lip_loc_range_t){
									.start = start_loc, .end = end_loc
								},
								"Invalid octal escape sequence"
							);
						}
						else
						{
							*outc++ = (char)num;
							if(has_non_oct_char) { *outc++ = *ch; }
						}
					}
					else
					{
						*outc++ = *ch;
					}
					break;
			}
		}
		else
		{
			*outc++ = *ch;
		}
	}
	*outc++ = '\0';

	string->data.string = (lip_string_ref_t){
		.length = outc - unescaped - 1,
		.ptr = unescaped
	};
	return lip_success(string);
}

static lip_ast_result_t
lip_translate_number(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_ast_t* number = lip_alloc_ast(allocator, sexp);
	number->type = LIP_AST_NUMBER;
	// Lexeme was already checked for error before
	number->data.number = strtod(sexp->data.string.ptr, NULL);
	return lip_success(number);
}

static lip_sexp_result_t
lip_quote(lip_allocator_t* allocator, const lip_sexp_t* sexp);

static lip_sexp_result_t
lip_quote_list(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;

	size_t length = lip_array_len(list);
	lip_array(lip_sexp_t) new_list = lip_array_create(allocator, lip_sexp_t, length + 1);
	lip_array_push(new_list, ((lip_sexp_t){
		.type =  LIP_SEXP_SYMBOL,
		.data = { .string = lip_string_ref("/list") }
	}));

	for(size_t i = 0; i < length; ++i)
	{
		lip_sexp_result_t result = lip_quote(allocator, &list[i]);
		if(!result.success) { return result; }

		lip_array_push(new_list, *result.value.result);
	}

	lip_sexp_t* list_sexp = lip_new(allocator, lip_sexp_t);
	*list_sexp = (lip_sexp_t){
		.type = LIP_SEXP_LIST,
		.data = { .list = new_list }
	};
	return lip_quote_success(list_sexp);
}

static lip_sexp_result_t
lip_quote(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	switch(sexp->type)
	{
		case LIP_SEXP_NUMBER:
		case LIP_SEXP_STRING:
			return lip_quote_success(sexp);
		case LIP_SEXP_SYMBOL:
			{
				lip_array(lip_sexp_t) quote_list =
					lip_array_create(allocator, lip_sexp_t, 2);
				lip_array_push(quote_list, ((lip_sexp_t){
					.type = LIP_SEXP_SYMBOL,
					.data = { .string = lip_string_ref("quote") }
				}));
				lip_array_push(quote_list, *sexp);

				lip_sexp_t* quote_sexp = lip_new(allocator, lip_sexp_t);
				*quote_sexp = (lip_sexp_t){
					.type = LIP_SEXP_LIST,
					.data = { .list = quote_list }
				};
				return lip_quote_success(quote_sexp);
			}
			break;
		case LIP_SEXP_LIST:
			return lip_quote_list(allocator, sexp);
	}

	return lip_quote_error(sexp->location, "Unknown error");
}

static lip_ast_result_t
lip_translate_quote(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;
	unsigned int arity = lip_array_len(list) - 1;

	ENSURE(
		arity == 1,
		"'quote' must have the form: (quote <exp>)"
	);

	lip_sexp_t* quoted_sexp = &list[1];
	switch(quoted_sexp->type)
	{
		case LIP_SEXP_LIST:
			{
				lip_sexp_result_t result = lip_quote_list(allocator, quoted_sexp);
				return result.success
					? lip_translate_sexp(allocator, result.value.result)
					: lip_syntax_error(
						result.value.error.location, result.value.error.extra
					);
			}
			break;
		case LIP_SEXP_SYMBOL:
			return lip_translate_symbol(allocator, quoted_sexp);
		case LIP_SEXP_STRING:
			return lip_translate_string(allocator, quoted_sexp);
		case LIP_SEXP_NUMBER:
			return lip_translate_number(allocator, quoted_sexp);
	}

	return lip_syntax_error(sexp->location, "Unknown error");
}

lip_ast_result_t
lip_translate_sexp(lip_allocator_t* allocator, const lip_sexp_t* sexp)
{
	switch(sexp->type)
	{
		case LIP_SEXP_LIST:
			if(lip_array_len(sexp->data.list) == 0)
			{
				return lip_syntax_error(sexp->location, "Empty list is invalid");
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
				else if(lip_string_ref_equal(symbol, lip_string_ref("quote")))
				{
					return lip_translate_quote(allocator, sexp);
				}
				else if(false
					|| lip_string_ref_equal(symbol, lip_string_ref("unquote"))
					|| lip_string_ref_equal(symbol, lip_string_ref("unquote-splicing"))
				)
				{
					return lip_syntax_error(
						sexp->location, "Cannot unquote outside of quote"
					);
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
	return lip_syntax_error(sexp->location, "Unknown error");
}
