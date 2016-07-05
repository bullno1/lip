#ifndef LIP_SEXP_H
#define LIP_SEXP_H

#include "common.h"

#define LIP_SEXP_TYPE(F) \
	F(LIP_SEXP_LIST) \
	F(LIP_SEXP_SYMBOL) \
	F(LIP_SEXP_STRING) \
	F(LIP_SEXP_NUMBER)

LIP_ENUM(lip_sexp_type_t, LIP_SEXP_TYPE)

typedef struct lip_sexp_s lip_sexp_t;

struct lip_sexp_s
{
	lip_sexp_type_t type;
	lip_loc_range_t location;
	union
	{
		lip_array(lip_sexp_t) list;
		lip_string_ref_t string;
	} data;
};

#endif
