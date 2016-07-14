#ifndef LIP_BUNDLER_H
#define LIP_BUNDLER_H

#include "common.h"
#include "asm.h"

typedef struct lip_bundler_s lip_bundler_t;

lip_bundler_t*
lip_bundler_create(lip_allocator_t* allocator);

void
lip_bundler_destroy(lip_bundler_t* bundler);

void
lip_bundler_begin(lip_bundler_t* bundler, lip_string_ref_t module_name);

lip_asm_index_t
lip_bundler_alloc_string_constant(lip_bundler_t* bundler, lip_string_ref_t str);

lip_asm_index_t
lip_bundler_alloc_numeric_constant(lip_bundler_t* bundler, double number);

lip_asm_index_t
lip_bundler_alloc_import(lip_bundler_t* bundler, lip_string_ref_t import);

void
lip_bundler_add_function(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	lip_function_t* function
);

void
lip_bundler_add_number(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	double value
);

void
lip_bundler_add_string(
	lip_bundler_t* bundler,
	bool exported,
	lip_string_ref_t name,
	lip_string_ref_t string
);

lip_module_t*
lip_bundler_end(lip_bundler_t* bundler);

#endif
