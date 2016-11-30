#include "ex/universe.h"
#include "utils.h"
#include "array.h"

__KHASH_IMPL(
	lip_namespace,
	LIP_MAYBE_UNUSED,
	lip_string_ref_t,
	lip_value_t,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_environment,
	LIP_MAYBE_UNUSED,
	lip_string_ref_t,
	khash_t(lip_namespace)*,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)

LIP_IMPLEMENT_CONSTRUCTOR_AND_DESTRUCTOR(lip_universe)

static lip_exec_status_t
lip_ns(lip_vm_t* vm)
{
	(void)vm;
	return LIP_EXEC_OK;
}

static void
lip_universe_open(lip_lni_t* lni)
{
	lip_universe_t* universe = LIP_CONTAINER_OF(lni, lip_universe_t, lni);
	lip_value_t env[] = {
		{ .type = LIP_VAL_NATIVE, .data = { .reference = universe } }
	};

	lip_native_function_t  functions[] = {
		{
			.name = lip_string_ref("ns"),
			.fn = lip_ns,
			.arity = 1,
			.environment = env,
			.env_len = LIP_STATIC_ARRAY_LEN(env)
		}
	};

	lip_lni_register(
		lni,
		lip_string_ref("(*universe*)"),
		functions, LIP_STATIC_ARRAY_LEN(functions)
	);
}

static lip_exec_status_t
lip_link_stub(lip_vm_t* vm)
{
	lip_value_t* args = lip_vm_get_args(vm, NULL);
	lip_string_t* symbol = args[0].data.reference;
	lip_import_t* import = args[1].data.reference;

	lip_value_t* env = lip_vm_get_env(vm, NULL);
	lip_universe_t* universe = env[0].data.reference;

	lip_value_t result;
	lip_string_ref_t symbol_ref = {
		.ptr = symbol->ptr, .length = symbol->length
	};
	if(lip_universe_find_symbol(universe, symbol_ref, &result))
	{
		lip_vm_push_value(vm, result);
		import->value = result;
		return LIP_EXEC_OK;
	}
	else
	{
		return LIP_EXEC_ERROR;
	}

	return LIP_EXEC_OK;
}

static khash_t(lip_namespace)*
lip_universe_find_namespace(lip_universe_t* universe, lip_string_ref_t name)
{
	khiter_t itr = kh_get(lip_environment, universe->environment, name);
	return itr == kh_end(universe->environment) ?
		NULL : kh_val(universe->environment, itr);
}

static bool
lip_namespace_find_symbol(
	khash_t(lip_namespace)* ns, lip_string_ref_t name, lip_value_t* result
)
{
	khiter_t itr = kh_get(lip_namespace, ns, name);
	if(itr == kh_end(ns))
	{
		return false;
	}
	else
	{
		*result = kh_val(ns, itr);
		return true;
	}
}

static void
lip_universe_register(
	lip_lni_t* lni,
	lip_string_ref_t namespace,
	lip_native_function_t* functions,
	uint32_t num_functions
)
{
	lip_universe_t* universe = LIP_CONTAINER_OF(lni, lip_universe_t, lni);
	khash_t(lip_namespace)* ns = lip_universe_find_namespace(universe, namespace);
	if(ns == NULL)
	{
		ns = kh_init(lip_namespace, universe->allocator);

		void* mem = lip_malloc(universe->allocator, namespace.length);
		memcpy(mem, namespace.ptr, namespace.length);
		lip_string_ref_t key = { .ptr = mem, .length = namespace.length };

		int ret;
		khiter_t itr = kh_put(lip_environment, universe->environment, key, &ret);
		kh_value(universe->environment, itr) = ns;
	}

	for(uint32_t i = 0; i < num_functions; ++i)
	{
		lip_native_function_t* fn = &functions[i];

		lip_value_t sym_val;
		if(lip_namespace_find_symbol(ns, fn->name, &sym_val)) { continue; }

		void* mem = lip_malloc(universe->allocator, fn->name.length);
		memcpy(mem, fn->name.ptr, fn->name.length);
		lip_string_ref_t key = { .ptr = mem, .length = fn->name.length };

		size_t env_size = sizeof(lip_value_t) * fn->env_len;
		lip_closure_t* closure = lip_malloc(
			universe->allocator,
			sizeof(lip_closure_t) + env_size
		);
		closure->is_native = true;
		closure->native_arity = fn->arity;
		closure->function.native = fn->fn;
		closure->env_len = fn->env_len;
		if(env_size)
		{
			memcpy(closure->environment, fn->environment, env_size);
		}

		int ret;
		khiter_t itr = kh_put(lip_namespace, ns, key, &ret);
		kh_value(ns, itr) = (lip_value_t){
			.type = LIP_VAL_FUNCTION,
			.data = { .reference = closure }
		};
	}
}

static lip_result_t
lip_transform_ns(
	lip_universe_t* universe,
	lip_allocator_t* allocator,
	lip_ast_t* ast
)
{
	(void)universe;

	LIP_AST_CHECK(
		lip_array_len(ast->data.application.arguments) == 1
		&& ast->data.application.arguments[0]->type == LIP_AST_IDENTIFIER,
		"'ns' must have the form: (ns <name>)"
	);

	ast->data.application.function->data.string = lip_string_ref("(*universe*)/ns");
	ast->data.application.arguments[0]->type = LIP_AST_STRING;

	return lip_success(ast);
}

static lip_result_t
lip_universe_transform_ast(
	lip_ast_transform_t* context,
	lip_allocator_t* allocator,
	lip_ast_t* ast
)
{
	if(ast->type != LIP_AST_APPLICATION) { return lip_success(ast); }
	if(ast->data.application.function->type != LIP_AST_IDENTIFIER)
	{
		return lip_success(ast);
	}

	lip_universe_t* universe =
		LIP_CONTAINER_OF(context, lip_universe_t, ast_transform);

	lip_string_ref_t fn_name = ast->data.application.function->data.string;
	if(lip_string_ref_equal(fn_name, lip_string_ref("ns")))
	{
		return lip_transform_ns(universe, allocator, ast);
	}
	else
	{
		return lip_success(ast);
	}
}

void
lip_universe_init(lip_universe_t* universe, lip_allocator_t* allocator)
{
	universe->allocator = allocator;
	universe->lni.reg = lip_universe_register;
	universe->ast_transform.transform = lip_universe_transform_ast;
	universe->environment = kh_init(lip_environment, allocator);
	lip_closure_t* link_stub =
		lip_malloc(allocator, sizeof(lip_closure_t) + sizeof(lip_value_t));
	link_stub->is_native = true;
	link_stub->env_len = 1;
	link_stub->environment[0].type = LIP_VAL_NATIVE;
	link_stub->environment[0].data.reference = universe;
	link_stub->native_arity = 2;
	link_stub->function.native = lip_link_stub;
	universe->link_stub = link_stub;

	lip_universe_open(&universe->lni);
}

void
lip_universe_cleanup(lip_universe_t* universe)
{
	for(
		khiter_t env_itr = kh_begin(universe->environment);
		env_itr != kh_end(universe->environment);
		++env_itr
	)
	{
		if(!kh_exist(universe->environment, env_itr)) { continue; }

		khash_t(lip_namespace)* ns = kh_val(universe->environment, env_itr);

		for(khiter_t ns_itr = kh_begin(ns); ns_itr != kh_end(ns); ++ns_itr)
		{
			if(!kh_exist(ns, ns_itr)) { continue; }

			lip_string_ref_t key = kh_key(ns, ns_itr);
			lip_value_t value = kh_val(ns, ns_itr);

			lip_free(universe->allocator, (void*)key.ptr);
			if(value.type == LIP_VAL_FUNCTION)
			{
				lip_free(universe->allocator, value.data.reference);
			}
		}

		lip_free(universe->allocator, (void*)(kh_key(universe->environment, env_itr).ptr));
		kh_destroy(lip_namespace, ns);
	}
	kh_destroy(lip_environment, universe->environment);

	lip_free(universe->allocator, universe->link_stub);
}

void
lip_universe_begin_load(lip_universe_t* universe, lip_function_t* function)
{
	lip_function_layout_t layout;
	lip_function_layout(function, &layout);

	for(uint16_t i = 0; i < function->num_imports; ++i)
	{
		lip_import_t* import = &layout.imports[i];
		import->value.type = LIP_VAL_PLACEHOLDER;
		import->value.data.reference = universe->link_stub;
	}

	// Recursively link nested functions
	for(uint16_t i = 0; i < function->num_functions; ++i)
	{
		lip_universe_begin_load(
			universe,
			lip_function_resource(function, layout.function_offsets[i])
		);
	}
}

void
lip_universe_end_load(lip_universe_t* universe, lip_function_t* function)
{
	(void)universe;
	(void)function;
}

lip_lni_t*
lip_universe_lni(lip_universe_t* universe)
{
	return &universe->lni;
}

bool
lip_universe_find_symbol(
	lip_universe_t* universe,
	lip_string_ref_t symbol,
	lip_value_t* result
)
{
	char* pos = memchr(symbol.ptr, '/', symbol.length);
	lip_string_ref_t target_namespace;
	lip_string_ref_t target_symbol;

	if(pos)
	{
		target_namespace.ptr = symbol.ptr;
		target_namespace.length = pos - symbol.ptr;
		target_symbol.ptr = pos + 1;
		target_symbol.length = symbol.length - target_namespace.length - 1;
	}
	else
	{
		target_namespace = lip_string_ref("");
		target_symbol = symbol;
	}

	khash_t(lip_namespace)* ns =
		lip_universe_find_namespace(universe, target_namespace);
	if(ns == NULL) { return false; }

	return lip_namespace_find_symbol(ns, target_symbol, result);
}

lip_ast_transform_t*
lip_universe_ast_transform(lip_universe_t* universe)
{
	return &universe->ast_transform;
}
