#ifndef LIP_CORE_PP_H
#define LIP_CORE_PP_H

#include "extra.h"
#include "sexp.h"

typedef struct lip_pp_s lip_pp_t;
typedef lip_error_m(lip_sexp_t*) lip_pp_result_t;

struct lip_pp_s
{
	lip_allocator_t* allocator;
};

LIP_CORE_API lip_pp_result_t
lip_preprocess(lip_pp_t* pp, lip_sexp_t* sexp);

#endif
