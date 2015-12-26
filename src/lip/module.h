#ifndef LIP_MODULE_H
#define LIP_MODULE_H

#include "value.h"
#include "types.h"

typedef struct lip_module_t
{
	size_t num_symbols;
	size_t symbol_section_size;
	lip_string_t** symbols;
	lip_value_t* values;
} lip_module_t;

void lip_module_print(lip_module_t* module);

#endif
