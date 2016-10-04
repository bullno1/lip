#include "ex/universe.h"
#include "utils.h"
#include "array.h"

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
	lip_string_t* symbol = lip_vm_get_arg(vm, 0).data.reference;
	lip_import_t* import = lip_vm_get_arg(vm, 1).data.reference;
	lip_universe_t* universe = lip_vm_get_arg(vm, -1).data.reference;

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

static lip_namespace_t*
lip_universe_find_namespace(lip_universe_t* universe, lip_string_ref_t name)
{
	lip_array_foreach(lip_namespace_t, namespace, universe->namespaces)
	{
		if(true
			&& namespace->name->length == name.length
			&& (name.ptr == NULL ||
				memcmp(namespace->name->ptr, name.ptr, name.length) == 0)
		)
		{
			return namespace;
		}
	}

	return NULL;
}

static lip_symbol_t*
lip_namespace_find_symbol(lip_namespace_t* namespace, lip_string_ref_t name)
{
	lip_array_foreach(lip_symbol_t, symbol, namespace->symbols)
	{
		if(true
			&& symbol->name->length == name.length
			&& memcmp(symbol->name->ptr, name.ptr, name.length) == 0
		)
		{
			return symbol;
		}
	}

	return NULL;
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
	lip_namespace_t* ns = lip_universe_find_namespace(universe, namespace);
	if(ns == NULL)
	{
		ns = lip_array_alloc(universe->namespaces);
		ns->symbols = lip_array_create(
			universe->allocator, lip_symbol_t, num_functions
		);
		ns->name = lip_malloc(
			universe->allocator, sizeof(lip_string_t) + namespace.length
		);
		ns->name->length = namespace.length;
		memcpy(ns->name->ptr, namespace.ptr, namespace.length);
	}

	for(uint32_t i = 0; i < num_functions; ++i)
	{
		lip_native_function_t* fn = &functions[i];
		lip_symbol_t* sym = lip_namespace_find_symbol(ns, fn->name);
		if(sym == NULL)
		{
			sym = lip_array_alloc(ns->symbols);
		}

		sym->name = lip_malloc(
			universe->allocator, sizeof(lip_string_t) + fn->name.length
		);
		sym->name->length = fn->name.length;
		memcpy(sym->name->ptr, fn->name.ptr, fn->name.length);

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
		sym->value.type = LIP_VAL_FUNCTION;
		sym->value.data.reference = closure;
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
	universe->namespaces = lip_array_create(allocator, lip_namespace_t, 1);
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
	lip_array_foreach(lip_namespace_t, namespace, universe->namespaces)
	{
		lip_array_foreach(lip_symbol_t, symbol, namespace->symbols)
		{
			lip_free(universe->allocator, symbol->name);
			if(symbol->value.type == LIP_VAL_FUNCTION)
			{
				lip_free(universe->allocator, symbol->value.data.reference);
			}
		}

		lip_array_destroy(namespace->symbols);
		lip_free(universe->allocator, namespace->name);
	}
	lip_free(universe->allocator, universe->link_stub);
	lip_array_destroy(universe->namespaces);
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
		target_namespace.ptr = NULL;
		target_namespace.length = 0;
		target_symbol = symbol;
	}

	lip_namespace_t* namespace =
		lip_universe_find_namespace(universe, target_namespace);
	if(namespace == NULL) { return false; }

	lip_symbol_t* sym = lip_namespace_find_symbol(namespace, target_symbol);
	if(sym == NULL) { return false; }

	*result = sym->value;
	return true;
}

lip_ast_transform_t*
lip_universe_ast_transform(lip_universe_t* universe)
{
	return &universe->ast_transform;
}
