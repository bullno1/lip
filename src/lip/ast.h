#ifndef LIP_AST_H
#define LIP_AST_H

#include "common.h"
#include "opcode.h"
#include "memory.h"
#include "sexp.h"

#define LIP_AST(F) \
	F(LIP_AST_NUMBER) \
	F(LIP_AST_STRING) \
	F(LIP_AST_IDENTIFIER) \
	F(LIP_AST_APPLICATION) \
	F(LIP_AST_IF) \
	F(LIP_AST_LET) \
	F(LIP_AST_LETREC) \
	F(LIP_AST_LAMBDA) \
	F(LIP_AST_DO)

LIP_ENUM(lip_ast_type_t, LIP_AST)

#define LIP_AST_CHECK(cond, msg) \
	do { \
		if(!(cond)) { \
			return lip_syntax_error(allocator, ast->location, msg); \
		} \
	} while(0)

typedef struct lip_ast_s lip_ast_t;
typedef struct lip_let_binding_s lip_let_binding_t;
typedef struct lip_result_s lip_result_t;
typedef struct lip_ast_transform_s lip_ast_transform_t;

struct lip_let_binding_s
{
	lip_string_ref_t name;
	lip_ast_t* value;
	lip_loc_range_t location;
};

struct lip_ast_s
{
	lip_ast_type_t type;
	lip_loc_range_t location;
	union
	{
		double number;
		lip_string_ref_t string;

		struct
		{
			lip_ast_t* function;
			lip_array(lip_ast_t*) arguments;
		} application;

		struct
		{
			lip_ast_t* condition;
			lip_ast_t* then;
			lip_ast_t* else_;
		} if_;

		struct
		{
			lip_array(lip_let_binding_t) bindings;
			lip_array(lip_ast_t*) body;
		} let;

		struct
		{
			lip_array(lip_string_ref_t) arguments;
			lip_array(lip_ast_t*) body;
		} lambda;

		lip_array(lip_ast_t*) do_;
	} data;
};

struct lip_result_s
{
	bool success;
	void* value;
};

struct lip_ast_transform_s
{
	lip_result_t(*transform)(
		lip_ast_transform_t* context, lip_allocator_t* allocator, lip_ast_t* ast
	);
};

lip_result_t
lip_translate_sexp(lip_allocator_t* allocator, const lip_sexp_t* sexp);

LIP_MAYBE_UNUSED static inline lip_result_t
lip_success(void* result)
{
	return (lip_result_t){
		.success = true,
		.value = result
	};
}

LIP_MAYBE_UNUSED static inline lip_result_t
lip_syntax_error(
	lip_allocator_t* allocator, lip_loc_range_t location, const char* msg
)
{
	lip_error_t* error = lip_new(allocator, lip_error_t);
	error->code = 0;
	error->extra = msg;
	error->location = location;
	return (lip_result_t){
		.success = false,
		.value = error
	};
}

LIP_MAYBE_UNUSED static inline lip_result_t
lip_transform_ast(
	lip_ast_transform_t* transform, lip_allocator_t* allocator, lip_ast_t* ast
)
{
	return transform->transform(transform, allocator, ast);
}

#endif
