#ifndef LIP_BUNDLER_H
#define LIP_BUNDLER_H

#include "types.h"

typedef struct lip_allocator_t lip_allocator_t;
typedef struct lip_module_t lip_module_t;
typedef struct lip_function_t lip_function_t;

typedef struct lip_bundler_t
{
	lip_allocator_t* allocator;
	lip_array(lip_string_ref_t) symbols;
	lip_array(lip_function_t*) functions;
} lip_bundler_t;

void lip_bundler_init(
	lip_bundler_t* bundler,
	lip_allocator_t* allocator
);
void lip_bundler_begin(lip_bundler_t* bundler);
void lip_bundler_add_function(
	lip_bundler_t* bundler,
	lip_string_ref_t name,
	lip_function_t* function
);
lip_module_t* lip_bundler_end(lip_bundler_t* bundler);
void lip_bundler_cleanup(lip_bundler_t* bundler);

#endif
