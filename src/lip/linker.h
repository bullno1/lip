#ifndef LIP_LINKER_H
#define LIP_LINKER_H

#include "types.h"
#include "value.h"

typedef struct lip_module_t lip_module_t;

typedef struct lip_linker_t
{
	lip_array(lip_module_t*) modules;
} lip_linker_t;

void lip_linker_init(lip_linker_t* linker, lip_allocator_t* allocator);
void lip_linker_add_module(lip_linker_t* linker, lip_module_t* module);
void lip_linker_find_symbol(
	lip_linker_t* linker, lip_string_ref_t symbol, lip_value_t* result
);
void lip_linker_link_modules(lip_linker_t* linker);
void lip_linker_cleanup(lip_linker_t* linker);

#endif
