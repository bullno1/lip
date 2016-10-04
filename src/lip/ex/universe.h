#ifndef LIP_UNIVERSE_EX_H
#define LIP_UNIVERSE_EX_H

#include "../universe.h"
#include "vm.h"

typedef struct lip_symbol_s lip_symbol_t;
typedef struct lip_namespace_s lip_namespace_t;

struct lip_symbol_s
{
	lip_string_t* name;
	lip_value_t value;
};

struct lip_namespace_s
{
	lip_string_t* name;
	lip_array(lip_symbol_t) symbols;
};

struct lip_universe_s
{
	lip_lni_t lni;
	lip_ast_transform_t ast_transform;
	lip_allocator_t* allocator;
	lip_array(lip_namespace_t) namespaces;
	lip_closure_t* link_stub;
};

void
lip_universe_init(lip_universe_t* universe, lip_allocator_t* allocator);

void
lip_universe_cleanup(lip_universe_t* universe);

#endif
