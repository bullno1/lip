#include "module.h"
#include <stdio.h>
#include "function.h"
#include "allocator.h"
#include "utils.h"

void lip_module_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_module_t* module, int max_depth
)
{
	for(unsigned int i = 0; i < module->num_symbols; ++i)
	{
		lip_printf(
			write_fn, ctx,
			"%.*s:\n", module->symbols[i]->length, module->symbols[i]->ptr
		);
		lip_value_print(write_fn, ctx, &module->values[i], max_depth, 1);
		lip_printf(write_fn, ctx, "\n");
	}
}

void lip_function_free(lip_allocator_t* allocator, lip_function_t* function)
{
	for(unsigned int i = 0; i < function->num_functions; ++i)
	{
		lip_function_free(allocator, function->functions[i]);
	}

	lip_free(allocator, function);
}

void lip_module_free(lip_allocator_t* allocator, lip_module_t* module)
{
	for(unsigned int i = 0; i < module->num_symbols; ++i)
	{
		lip_value_t* value = &module->values[i];
		if(value->type != LIP_VAL_CLOSURE) { continue; }

		lip_closure_t* closure = (lip_closure_t*)value->data.reference;
		if(closure->info.is_native) { continue; }

		lip_function_free(allocator, closure->function_ptr.lip);
	}

	lip_free(allocator, module);
}
