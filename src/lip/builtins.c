#include "lip_internal.h"
#include <lip/lip.h>
#include <lip/vm.h>
#include <lip/print.h>
#include <lip/io.h>
#include <lip/bind.h>
#include <setjmp.h>
#include "vendor/sort_r.h"
#include "prim_ops.h"

struct lip_cmp_ctx
{
	lip_vm_t* vm;
	lip_value_t cmp_fn;
	lip_value_t error;
	jmp_buf err_jmp_buf;
};

static lip_function(nop)
{
	(void)vm;
	(void)result;
	lip_return(lip_make_nil(vm));
}

static lip_function(identity)
{
	lip_bind_args((any, x));
	lip_return(x);
}

static lip_function(print)
{
	lip_bind_args(
		(any, x),
		(number, depth, (optional, 3)),
		(number, indent, (optional, 0))
	);
	lip_print_value(depth, indent, lip_stdout(), x);
	lip_return(lip_make_nil(vm));
}

static lip_function(throw)
{
	lip_bind_args((string, msg));
	*result = msg;
	return LIP_EXEC_ERROR;
}

// Type test

static lip_function(is_nil)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_NIL));
}

static lip_function(is_bool)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_BOOLEAN));
}

static lip_function(is_number)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_NUMBER));
}

static lip_function(is_string)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_STRING));
}

static lip_function(is_symbol)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_SYMBOL));
}

static lip_function(is_list)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_LIST));
}

static lip_function(is_fn)
{
	lip_bind_args((any, x));
	lip_return(lip_make_boolean(vm, x.type == LIP_VAL_FUNCTION));
}

static lip_function(declare)
{
	lip_bind_args((symbol, name), (boolean, public), (function, fn));
	lip_runtime_link_t* rt = LIP_CONTAINER_OF(vm->rt, lip_runtime_link_t, vtable);

	khash_t(lip_ns)* ns = rt->ctx->current_module;
	lip_bind_assert(ns != NULL, "Cannot use `declare` out of module context");

	lip_closure_t* closure = fn.data.reference;
	lip_bind_assert(closure->env_len == 0, "Cannot `declare` function with captured var");

	lip_string_t* name_str = lip_as_string(name);
	lip_string_ref_t name_ref = { .length = name_str->length, .ptr = name_str->ptr };
	int ret;
	khiter_t itr = kh_put(lip_ns, ns, name_ref, &ret);
	lip_bind_assert_fmt(ret != 0, "Redeclared '%.*s", (int)name_str->length, name_str->ptr);
	kh_val(ns, itr) = (lip_symbol_t){
		.value = fn.data.reference,
		.is_public = public
	};

	lip_return(lip_make_nil(vm));
}

// List functions
static lip_function(list)
{
	lip_bind_prepare(vm);

	lip_list_t* list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	list->root = list->elements =
		vm->rt->malloc(vm->rt, LIP_VAL_NATIVE, sizeof(lip_value_t) * argc);
	list->length = argc;
	for(unsigned int i = 0; i < argc; ++i)
	{
		lip_bind_arg(i + 1, (any, element));

		list->elements[i] = element;
	}

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = list }
	};
	lip_return(ret_val);
}

static lip_function(head)
{
	lip_bind_args((list, x));
	const lip_list_t* list = lip_as_list(x);
	lip_bind_assert(list->length > 0, "List must have at least one element");

	lip_return(list->elements[0]);
}

static lip_function(tail)
{
	lip_bind_args((list, x));
	const lip_list_t* list = lip_as_list(x);
	lip_bind_assert(list->length > 0, "List must have at least one element");

	lip_list_t* new_list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	new_list->length = list->length - 1;
	new_list->root = list->root;
	new_list->elements = list->elements + 1;

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = new_list }
	};
	lip_return(ret_val);
}

static lip_function(len)
{
	lip_bind_args((list, x));
	lip_return(lip_make_number(vm, lip_as_list(x)->length));
}

static lip_function(nth)
{
	lip_bind_args((number, index), (list, x));
	const lip_list_t* list = lip_as_list(x);
	lip_bind_assert(0 <= index && index < list->length, "List index out of bound");
	lip_return(lip_as_list(x)->elements[(int)index]);
}

static lip_function(concat)
{
	lip_bind_prepare(vm);

	size_t length = 0;

	for(unsigned int i = 0; i < argc; ++i)
	{
		lip_bind_arg(i + 1, (list, list));
		length += lip_as_list(list)->length;
	}

	lip_list_t* list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	list->root = list->elements =
		vm->rt->malloc(vm->rt, LIP_VAL_NATIVE, sizeof(lip_value_t) * length);
	list->length = length;

	size_t index = 0;
	for(unsigned int i = 0; i < argc; ++i)
	{
		const lip_list_t* sublist = lip_as_list(argv[i]);
		memcpy(
			&list->elements[index],
			sublist->elements,
			sublist->length * sizeof(*sublist->elements)
		);
		index += sublist->length;
	}

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = list }
	};
	lip_return(ret_val);
}

static lip_function(append)
{
	lip_bind_args((list, l), (any, x));

	const lip_list_t* list = lip_as_list(l);

	lip_list_t* new_list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	new_list->root = new_list->elements =
		vm->rt->malloc(vm->rt, LIP_VAL_NATIVE, sizeof(lip_value_t) * (list->length + 1));
	new_list->length = list->length + 1;

	memcpy(new_list->elements, list->elements, sizeof(lip_value_t) * list->length);
	new_list->elements[list->length] = x;

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = new_list }
	};
	lip_return(ret_val);
}

static lip_function(map)
{
	lip_bind_args((function, f), (list, l));

	const lip_list_t* list = lip_as_list(l);

	lip_list_t* new_list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	new_list->root = new_list->elements =
		vm->rt->malloc(vm->rt, LIP_VAL_NATIVE, sizeof(lip_value_t) * list->length);
	new_list->length = list->length;

	for(size_t i = 0; i < list->length; ++i)
	{
		lip_exec_status_t status =
			lip_call(vm, &new_list->elements[i], f, 1, list->elements[i]);
		if(status != LIP_EXEC_OK)
		{
			*result = new_list->elements[i];
			return status;
		}
	}

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = new_list }
	};
	lip_return(ret_val);
}

static lip_function(foldl)
{
	lip_bind_args((function, f), (list, l), (any, acc));

	const lip_list_t* list = lip_as_list(l);

	for(size_t i = 0; i < list->length; ++i)
	{
		lip_exec_status_t status =
			lip_call(vm, &acc, f, 2, list->elements[i], acc);
		if(status != LIP_EXEC_OK)
		{
			*result = acc;
			return status;
		}
	}

	lip_return(acc);
}

static lip_function(foldr)
{
	lip_bind_args((function, f), (list, l), (any, acc));

	const lip_list_t* list = lip_as_list(l);

	for(size_t i = 0; i < list->length; ++i)
	{
		lip_exec_status_t status =
			lip_call(vm, &acc, f, 2, list->elements[list->length - i - 1], acc);
		if(status != LIP_EXEC_OK)
		{
			*result = acc;
			return status;
		}
	}

	lip_return(acc);
}

#define LIP_PRIM_OP_WRAPPER_NAME(name) \
	lip_pp_concat(LIP_PRIM_OP_FN_NAME(name), _wrapper)
#define LIP_WRAP_PRIM_OP(op, name) \
	static lip_function(lip_pp_concat(LIP_PRIM_OP_FN_NAME(name), _wrapper)) { \
		lip_bind_prepare(vm); \
		return LIP_PRIM_OP_FN_NAME(name)(vm, result, argc, argv); \
	}
LIP_PRIM_OP(LIP_WRAP_PRIM_OP)

static lip_exec_status_t
lip_call_cmp_fn(
	lip_vm_t* vm,
	lip_value_t* result,
	lip_value_t cmp_fn,
	lip_value_t lhs,
	lip_value_t rhs)
{
	lip_exec_status_t status = lip_call(vm, result, cmp_fn, 2, lhs, rhs);
	if(status != LIP_EXEC_OK) { return status; }

	lip_bind_assert(
		result->type == LIP_VAL_NUMBER,
		"Comparision function did not return a number"
	);

	return status;
}

static int
lip_cmp_fn(
	const void* lhs,
	const void* rhs,
	void* ctx_
)
{
	struct lip_cmp_ctx* ctx = ctx_;

	lip_value_t result;
	lip_exec_status_t status = lip_call_cmp_fn(
		ctx->vm, &result, ctx->cmp_fn, *(lip_value_t*)lhs, *(lip_value_t*)rhs
	);
	if(status != LIP_EXEC_OK)
	{
		ctx->error = result;
		longjmp(ctx->err_jmp_buf, status);
	}

	return (int)result.data.number;
}

static lip_function(sort)
{
	lip_bind_args(
		(list, l),
		(function, cmp,
		 (optional, (lip_make_function(vm, LIP_PRIM_OP_WRAPPER_NAME(CMP), 0, NULL))))
	);

	const lip_list_t* list = lip_as_list(l);

	lip_list_t* new_list = vm->rt->malloc(vm->rt, LIP_VAL_LIST, sizeof(lip_list_t));
	new_list->root = new_list->elements = vm->rt->malloc(
		vm->rt, LIP_VAL_NATIVE, sizeof(lip_value_t) * list->length
	);
	new_list->length = list->length;

	memcpy(new_list->elements, list->elements, sizeof(lip_value_t) * list->length);

	struct lip_cmp_ctx cmp_ctx = {
		.vm = vm,
		.cmp_fn = cmp
	};

	if(list->length > 0)
	{
		if(setjmp(cmp_ctx.err_jmp_buf) == 0)
		{
			sort_r(
				new_list->elements,
				list->length,
				sizeof(lip_value_t),
				lip_cmp_fn,
				&cmp_ctx
			);
		}
		else
		{
			*result = cmp_ctx.error;
			return LIP_EXEC_ERROR;
		}
	}

	lip_value_t ret_val = (lip_value_t) {
		.type = LIP_VAL_LIST,
		.data = { .reference = new_list }
	};
	lip_return(ret_val);
}

void
lip_load_builtins(lip_context_t* ctx)
{
	lip_ns_context_t* ns = lip_begin_ns(ctx, lip_string_ref(""));
	lip_declare_function(ns, lip_string_ref("nop"), nop);
	lip_declare_function(ns, lip_string_ref("identity"), identity);
	lip_declare_function(ns, lip_string_ref("print"), print);
	lip_declare_function(ns, lip_string_ref("throw"), throw);
	lip_declare_function(ns, lip_string_ref("list"), list);
	lip_declare_function(ns, lip_string_ref("declare"), declare);

	lip_declare_function(ns, lip_string_ref("nil?"), is_nil);
	lip_declare_function(ns, lip_string_ref("bool?"), is_bool);
	lip_declare_function(ns, lip_string_ref("number?"), is_number);
	lip_declare_function(ns, lip_string_ref("string?"), is_string);
	lip_declare_function(ns, lip_string_ref("symbol?"), is_symbol);
	lip_declare_function(ns, lip_string_ref("list?"), is_list);
	lip_declare_function(ns, lip_string_ref("fn?"), is_fn);

#define LIP_STRINGIFY(x) LIP_STRINGIFY1(x)
#define LIP_STRINGIFY1(x) #x
#define LIP_REGISTER_PRIM_OP(op, name) \
	lip_declare_function(ns, lip_string_ref(LIP_STRINGIFY(op)), LIP_PRIM_OP_WRAPPER_NAME(name));
	LIP_PRIM_OP(LIP_REGISTER_PRIM_OP)
#undef LIP_REGISTER_PRIM_OP
	lip_end_ns(ctx, ns);

	ns = lip_begin_ns(ctx, lip_string_ref("list"));
	lip_declare_function(ns, lip_string_ref("head"), head);
	lip_declare_function(ns, lip_string_ref("tail"), tail);
	lip_declare_function(ns, lip_string_ref("len"), len);
	lip_declare_function(ns, lip_string_ref("nth"), nth);
	lip_declare_function(ns, lip_string_ref("append"), append);
	lip_declare_function(ns, lip_string_ref("concat"), concat);
	lip_declare_function(ns, lip_string_ref("map"), map);
	lip_declare_function(ns, lip_string_ref("foldl"), foldl);
	lip_declare_function(ns, lip_string_ref("foldr"), foldr);
	lip_declare_function(ns, lip_string_ref("sort"), sort);
	lip_end_ns(ctx, ns);
}
