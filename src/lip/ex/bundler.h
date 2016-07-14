#ifndef LIP_BUNDLER_EX_H
#define LIP_BUNDLER_EX_H

#include "../bundler.h"
#include "../memory.h"
#include "vm.h"

struct lip_bundler_s
{
	lip_allocator_t* allocator;
	lip_array(lip_kv_t) public_symbols;
	lip_array(lip_kv_t) private_symbols;
	lip_array(uint32_t) imports;
	lip_array(lip_value_t) const_pool;
	lip_array(lip_string_ref_t) string_pool;
	lip_array(lip_function_t*) functions;
	lip_array(lip_memblock_info_t) string_layout;
	lip_array(lip_memblock_info_t) function_layout;
	lip_array(lip_memblock_info_t*) module_layout;
};

void
lip_bundler_init(lip_bundler_t* bundler, lip_allocator_t* allocator);

void
lip_bundler_cleanup(lip_bundler_t* bundler);

#endif
