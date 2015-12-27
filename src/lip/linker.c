#include "linker.h"
#include <string.h>
#include "array.h"
#include "module.h"
#include "function.h"

void lip_linker_init(lip_linker_t* linker, lip_allocator_t* allocator)
{
	linker->modules = lip_array_new(allocator);
}

void lip_linker_add_module(lip_linker_t* linker, lip_module_t* module)
{
	lip_array_push(linker->modules, module);
}

static inline void lip_linker_find_symbol_common(
	lip_linker_t* linker, size_t length, const char* ptr, lip_value_t* result
)
{
	lip_array_foreach(lip_module_t*, itr, linker->modules)
	{
		lip_module_t* module = *itr;
		for(size_t sym_index = 0; sym_index < module->num_symbols; ++sym_index)
		{
			lip_string_t* current_sym = module->symbols[sym_index];
			if(
				current_sym->length == length
				&& memcmp(current_sym->ptr, ptr, length) == 0
			)
			{
				*result = module->values[sym_index];
				return;
			}
		}
	}

	result->type = LIP_VAL_NIL;
}

static void lip_linker_link_function(
	lip_linker_t* linker, lip_function_t* function
)
{
	for(
		uint16_t import_index = 0;
		import_index < function->num_imports;
		++import_index
	)
	{
		lip_string_t* symbol = function->import_symbols[import_index];
		lip_linker_find_symbol_common(
			linker,
			symbol->length,
			symbol->ptr,
			&function->import_values[import_index]
		);
	}
}

static void lip_linker_link_module(lip_linker_t* linker, lip_module_t* module)
{
	for(size_t sym_index = 0; sym_index < module->num_symbols; ++sym_index)
	{
		lip_value_t* value = module->values + sym_index;
		if(
			value->type == LIP_VAL_CLOSURE
			&& !((lip_closure_t*)value->data.reference)->info.is_native
		)
		{
			lip_closure_t* closure = (lip_closure_t*)value->data.reference;
			lip_function_t* function = closure->function_ptr.lip;
			lip_linker_link_function(linker, function);
		}
	}
}

void lip_linker_link_modules(lip_linker_t* linker)
{
	lip_array_foreach(lip_module_t*, itr, linker->modules)
	{
		lip_linker_link_module(linker, *itr);
	}
}

void lip_linker_find_symbol(
	lip_linker_t* linker, lip_string_ref_t symbol, lip_value_t* result
)
{
	lip_linker_find_symbol_common(linker, symbol.length, symbol.ptr, result);
}

void lip_linker_cleanup(lip_linker_t* linker)
{
	lip_array_delete(linker->modules);
}
