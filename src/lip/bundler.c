#include "bundler.h"
#include <string.h>
#include "module.h"
#include "array.h"
#include "allocator.h"
#include "function.h"

void lip_bundler_init(
	lip_bundler_t* bundler,
	lip_allocator_t* allocator
)
{
	bundler->allocator = allocator;
	bundler->symbols = lip_array_new(allocator);
	bundler->functions = lip_array_new(allocator);
}

void lip_bundler_begin(lip_bundler_t* bundler)
{
	lip_array_clear(bundler->symbols);
	lip_array_clear(bundler->functions);
}

void lip_bundler_add_function(
	lip_bundler_t* bundler,
	lip_string_ref_t name,
	lip_function_t* function
)
{
	lip_array_push(bundler->symbols, name);
	lip_array_push(bundler->functions, function);
}

lip_module_t* lip_bundler_end(lip_bundler_t* bundler)
{
	// Calculate the size of the memory block for the module
	size_t header_size = sizeof(lip_module_t);
	size_t num_symbols = lip_array_len(bundler->symbols);
	size_t symbol_table_size = num_symbols * sizeof(lip_string_t*);
	size_t symbol_section_size = 0;
	lip_array_foreach(lip_string_ref_t, itr, bundler->symbols)
	{
		size_t entry_size = sizeof(lip_string_t) + itr->length;
		// align to void* size
		symbol_section_size +=
			(entry_size + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
	}
	size_t value_table_size = num_symbols * sizeof(lip_value_t);
	size_t closure_section_size = num_symbols * sizeof(lip_closure_t);

	size_t block_size =
		  header_size
		+ symbol_table_size
		+ symbol_section_size
		+ value_table_size
		+ closure_section_size;

	lip_module_t* module = lip_malloc(bundler->allocator, block_size);
	// Write header
	module->num_symbols = num_symbols;
	module->symbol_section_size = symbol_section_size;
	char* ptr = (char*)module + header_size;

	// Write symbol table
	module->symbols = (lip_string_t**)ptr;
	ptr += symbol_table_size;

	// Write value table
	module->values = (lip_value_t*)ptr;
	ptr += value_table_size;

	// Write symbol section
	for(size_t i = 0; i < num_symbols; ++i)
	{
		lip_string_ref_t* symbol = bundler->symbols + i;
		lip_string_t* entry = (lip_string_t*)ptr;
		entry->length = symbol->length;
		memcpy(&entry->ptr, symbol->ptr, symbol->length);
		module->symbols[i] = (lip_string_t*)ptr;

		size_t entry_size = sizeof(lip_string_t) + symbol->length;
		ptr +=
			(entry_size + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
	}

	// Write value section
	for(size_t i = 0; i < num_symbols; ++i)
	{
		lip_closure_t* entry = (lip_closure_t*)ptr;
		entry->info.is_native = false;
		entry->function_ptr.lip = bundler->functions[i];
		module->values[i].type = LIP_VAL_CLOSURE;
		module->values[i].data.reference = (lip_closure_t*)ptr;
		ptr += sizeof(lip_closure_t);
	}

	return module;
}

void lip_bundler_cleanup(lip_bundler_t* bundler)
{
	lip_array_delete(bundler->functions);
	lip_array_delete(bundler->symbols);
}
