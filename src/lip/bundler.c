#include "ex/bundler.h"
#include <stdlib.h>
#include "ex/vm.h"
#include "array.h"
#include "utils.h"

LIP_IMPLEMENT_CONSTRUCTOR_AND_DESTRUCTOR(lip_bundler)

void
lip_bundler_init(lip_bundler_t* bundler, lip_allocator_t* allocator)
{
	bundler->allocator = allocator;
	bundler->public_symbols = lip_array_create(allocator, lip_kv_t, 0);
	bundler->private_symbols = lip_array_create(allocator, lip_kv_t, 0);
	bundler->imports = lip_array_create(allocator, uint32_t, 0);
	bundler->const_pool = lip_array_create(allocator, lip_value_t, 0);
	bundler->string_pool = lip_array_create(allocator, lip_string_ref_t, 0);
	bundler->functions = lip_array_create(allocator, lip_function_t*, 0);
	bundler->string_layout = lip_array_create(allocator, lip_memblock_info_t, 0);
	bundler->function_layout = lip_array_create(allocator, lip_memblock_info_t, 0);
	bundler->module_layout = lip_array_create(allocator, lip_memblock_info_t*, 0);
}

void
lip_bundler_cleanup(lip_bundler_t* bundler)
{
	lip_array_destroy(bundler->module_layout);
	lip_array_destroy(bundler->function_layout);
	lip_array_destroy(bundler->string_layout);
	lip_array_destroy(bundler->functions);
	lip_array_destroy(bundler->string_pool);
	lip_array_destroy(bundler->const_pool);
	lip_array_destroy(bundler->imports);
	lip_array_destroy(bundler->private_symbols);
	lip_array_destroy(bundler->public_symbols);
}

static uint32_t
lip_bundler_alloc_string(lip_bundler_t* bundler, lip_string_ref_t string)
{
	uint32_t index = lip_array_len(bundler->string_pool);
	lip_array_push(bundler->string_pool, string);
	lip_array_push(bundler->string_layout, ((lip_memblock_info_t){
		.element_size = sizeof(lip_string_t) + string.length,
		.num_elements = 1,
		.alignment = lip_string_t_alignment
	}));

	return index;
}

void
lip_bundler_begin(lip_bundler_t* bundler, lip_string_ref_t module_name)
{
	lip_array_clear(bundler->public_symbols);
	lip_array_clear(bundler->private_symbols);
	lip_array_clear(bundler->imports);
	lip_array_clear(bundler->const_pool);
	lip_array_clear(bundler->string_pool);
	lip_array_clear(bundler->functions);
	lip_array_clear(bundler->string_layout);
	lip_array_clear(bundler->function_layout);
	lip_array_clear(bundler->module_layout);

	lip_bundler_alloc_string(bundler, module_name);
}

lip_asm_index_t
lip_bundler_alloc_string_constant(lip_bundler_t* bundler, lip_string_ref_t str)
{
	for(lip_asm_index_t i = 0; i < lip_array_len(bundler->const_pool); ++i)
	{
		if(true
			&& bundler->const_pool[i].type == LIP_VAL_STRING
			&& lip_string_ref_equal(
				str, bundler->string_pool[bundler->const_pool[i].data.index]))
		{
			return i;
		}
	}

	lip_asm_index_t index = lip_array_len(bundler->const_pool);
	lip_array_push(bundler->const_pool, ((lip_value_t) {
		.type = LIP_VAL_STRING,
		.data = { .index = lip_bundler_alloc_string(bundler, str) }
	}));
	return index;
}

lip_asm_index_t
lip_bundler_alloc_numeric_constant(lip_bundler_t* bundler, double number)
{
	for(lip_asm_index_t i = 0; i < lip_array_len(bundler->const_pool); ++i)
	{
		if(true
			&& bundler->const_pool[i].type == LIP_VAL_NUMBER
			&& bundler->const_pool[i].data.number == number)
		{
			return i;
		}
	}

	lip_asm_index_t index = lip_array_len(bundler->const_pool);
	lip_array_push(bundler->const_pool, ((lip_value_t) {
		.type = LIP_VAL_NUMBER,
		.data = { .number = number }
	}));
	return index;
}

lip_asm_index_t
lip_bundler_alloc_import(lip_bundler_t* bundler, lip_string_ref_t import)
{
	for(lip_asm_index_t i = 0; i < lip_array_len(bundler->imports); ++i)
	{
		if(lip_string_ref_equal(import, bundler->string_pool[bundler->imports[i]]))
		{
			return i;
		}
	}

	lip_asm_index_t index = lip_array_len(bundler->imports);
	lip_array_push(bundler->imports, lip_bundler_alloc_string(bundler, import));
	return index;
}

void
lip_bundler_add_function(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	lip_function_t* function
)
{
	uint32_t index = lip_array_len(bundler->functions);
	lip_array_push(bundler->functions, function);
	lip_array_push(bundler->function_layout, ((lip_memblock_info_t){
		.element_size = function->size,
		.num_elements = 1,
		.alignment = lip_function_t_alignment
	}));

	lip_array(lip_kv_t)* array = exported ?
		&bundler->public_symbols : &bundler->private_symbols;
	lip_array_push(*array, ((lip_kv_t) {
		.key = lip_bundler_alloc_string(bundler, name),
		.value = {
			.type = LIP_VAL_FUNCTION,
			.data = { .index = index }
		}
	}));
}

void
lip_bundler_add_number(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	double value
)
{
	lip_array(lip_kv_t)* array = exported ?
		&bundler->public_symbols : &bundler->private_symbols;
	lip_array_push(*array, ((lip_kv_t) {
		.key = lip_bundler_alloc_string(bundler, name),
		.value = {
			.type = LIP_VAL_NUMBER,
			.data = { .number = value }
		}
	}));
}

void
lip_bundler_add_string(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	lip_string_ref_t string
)
{
	lip_array(lip_kv_t)* array = exported ?
		&bundler->public_symbols : &bundler->private_symbols;
	lip_array_push(*array, ((lip_kv_t) {
		.key = lip_bundler_alloc_string(bundler, name),
		.value = {
			.type = LIP_VAL_STRING,
			.data = { .index = lip_bundler_alloc_string(bundler, string) }
		}
	}));
}

static void
lip_bundler_copy_symbols(
	lip_bundler_t* bundler,
	lip_kv_t* dst,
	lip_kv_t* src,
	uint32_t num_symbols
)
{
	for(uint32_t i = 0; i < num_symbols; ++i)
	{
		lip_kv_t* target_symbol = &dst[i];
		lip_kv_t* source_symbol = &src[i];
		lip_value_t symbol_value = source_symbol->value;
		target_symbol->key = bundler->string_layout[source_symbol->key].offset;

		target_symbol->value.type = source_symbol->value.type;
		switch(source_symbol->value.type)
		{
			case LIP_VAL_FUNCTION:
				target_symbol->value.data.index =
					bundler->function_layout[symbol_value.data.index].offset;
				break;
			case LIP_VAL_NUMBER:
				target_symbol->value.data.number = symbol_value.data.number;
				break;
			case LIP_VAL_STRING:
				target_symbol->value.data.index =
					bundler->string_layout[symbol_value.data.index].offset;
				break;
			default:
				// impossibru!!
				abort();
				break;
		}
	}
}

lip_module_t*
lip_bundler_end(lip_bundler_t* bundler)
{
	lip_memblock_info_t header_block = LIP_ARRAY_BLOCK(lip_module_t, 1);
	lip_array_push(bundler->module_layout, &header_block);

	lip_memblock_info_t public_symbol_block =
		LIP_ARRAY_BLOCK(lip_kv_t, lip_array_len(bundler->public_symbols));
	lip_array_push(bundler->module_layout, &public_symbol_block);

	lip_memblock_info_t private_symbol_block =
		LIP_ARRAY_BLOCK(lip_kv_t, lip_array_len(bundler->private_symbols));
	lip_array_push(bundler->module_layout, &private_symbol_block);

	lip_memblock_info_t import_block =
		LIP_ARRAY_BLOCK(lip_kv_t, lip_array_len(bundler->imports));
	lip_array_push(bundler->module_layout, &import_block);

	lip_memblock_info_t constant_block =
		LIP_ARRAY_BLOCK(lip_value_t, lip_array_len(bundler->const_pool));
	lip_array_push(bundler->module_layout, &constant_block);

	lip_array_foreach(lip_memblock_info_t, string_block, bundler->string_layout)
	{
		lip_array_push(bundler->module_layout, string_block);
	}

	lip_array_foreach(lip_memblock_info_t, block, bundler->function_layout)
	{
		lip_array_push(bundler->module_layout, block);
	}

	lip_memblock_info_t module_info = lip_align_memblocks(
		lip_array_len(bundler->module_layout), bundler->module_layout
	);
	lip_module_t* module = lip_malloc(bundler->allocator, module_info.num_elements);
	memset(module, 0, module_info.num_elements);

	module->num_public_symbols = lip_array_len(bundler->public_symbols);
	module->num_private_symbols = lip_array_len(bundler->private_symbols);
	module->num_imports = lip_array_len(bundler->imports);
	module->num_constants = lip_array_len(bundler->const_pool);

	lip_kv_t* public_symbols = lip_locate_memblock(module, &public_symbol_block);
	lip_bundler_copy_symbols(
		bundler,
		public_symbols,
		bundler->public_symbols,
		module->num_public_symbols
	);

	lip_kv_t* private_symbols = lip_locate_memblock(module, &private_symbol_block);
	lip_bundler_copy_symbols(
		bundler,
		private_symbols,
		bundler->private_symbols,
		module->num_private_symbols
	);

	lip_kv_t* imports = lip_locate_memblock(module, &import_block);
	for(uint32_t i = 0; i < module->num_imports; ++i)
	{
		lip_kv_t* import = &imports[i];
		import->key = bundler->string_layout[bundler->imports[i]].offset,
		import->value.type = LIP_VAL_NIL;
	}

	lip_value_t* constants = lip_locate_memblock(module, &constant_block);
	for(uint32_t i = 0; i < module->num_constants; ++i)
	{
		lip_value_t* target = &constants[i];
		lip_value_t* source = &bundler->const_pool[i];
		target->type = source->type;
		switch(target->type)
		{
			case LIP_VAL_NUMBER:
				target->data.number = source->data.number;
				break;
			case LIP_VAL_STRING:
				target->data.index =
					bundler->string_layout[source->data.index].offset;
				break;
			default:
				// impossibru!!
				abort();
				break;
		}
	}

	uint32_t num_strings = lip_array_len(bundler->string_pool);
	for(uint32_t i = 0; i < num_strings; ++i)
	{
		lip_string_t* target =
			lip_locate_memblock(module, &bundler->string_layout[i]);
		lip_string_ref_t source = bundler->string_pool[i];
		target->length = source.length;
		memcpy(target->ptr, source.ptr, source.length);
	}

	uint32_t num_functions = lip_array_len(bundler->functions);
	for(uint32_t i = 0; i < num_functions; ++i)
	{
		lip_function_t* target =
			lip_locate_memblock(module, &bundler->function_layout[i]);
		lip_function_t* source = bundler->functions[i];
		memcpy(target, source, source->size);
	}

	return module;
}
