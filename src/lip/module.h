#ifndef LIP_MODULE_H
#define LIP_MODULE_H

#include "value.h"
#include "types.h"

typedef struct lip_module_t
{
	uint32_t num_symbols;
	lip_string_t** symbols;
	lip_value_t* values;
} lip_module_t;

void lip_module_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_module_t* module, int max_depth
);
void lip_module_free(lip_allocator_t* allocator, lip_module_t* module);

#endif
