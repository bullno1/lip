#ifndef LIP_SEXP_H
#define LIP_SEXP_H

#include <stdlib.h>
#include "types.h"
#include "enum.h"

#define LIP_SEXP(F) \
	F(LIP_SEXP_LIST) \
	F(LIP_SEXP_SYMBOL) \
	F(LIP_SEXP_STRING) \
	F(LIP_SEXP_NUMBER)

LIP_ENUM(lip_sexp_type_t, LIP_SEXP)

typedef struct lip_allocator_t lip_allocator_t;

typedef struct lip_sexp_string_t
{
	const char* ptr;
	size_t length;
} lip_sexp_string_t;

typedef struct lip_sexp_t
{
	lip_sexp_type_t type;
	lip_loc_t start;
	lip_loc_t end;
	union
	{
		struct lip_sexp_t* list;
		lip_sexp_string_t string;
	} data;
} lip_sexp_t;

void lip_sexp_print(lip_sexp_t* sexp, int indent);
void lip_sexp_cleanup(lip_sexp_t* sexp);

#endif
