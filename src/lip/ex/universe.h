#ifndef LIP_UNIVERSE_EX_H
#define LIP_UNIVERSE_EX_H

#include "../universe.h"
#include "vm.h"
#include "../vendor/khash.h"

KHASH_DECLARE(lip_namespace, lip_string_ref_t, lip_value_t)
KHASH_DECLARE(lip_environment, lip_string_ref_t, khash_t(lip_namespace)*)

struct lip_universe_s
{
	lip_lni_t lni;
	lip_ast_transform_t ast_transform;
	lip_allocator_t* allocator;
	khash_t(lip_environment)* environment;
	lip_closure_t* link_stub;
};

void
lip_universe_init(lip_universe_t* universe, lip_allocator_t* allocator);

void
lip_universe_cleanup(lip_universe_t* universe);

#endif
