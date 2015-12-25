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

typedef struct lip_sexp_list_node_t lip_sexp_list_node_t;

typedef struct lip_sexp_list_t
{
	size_t length;
	lip_sexp_list_node_t* first;
	lip_sexp_list_node_t* last;
} lip_sexp_list_t;

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
		lip_sexp_list_t list;
		lip_sexp_string_t string;
	} data;
} lip_sexp_t;

typedef struct lip_sexp_list_node_t
{
	lip_sexp_t data;
	struct lip_sexp_list_node_t* next;
} lip_sexp_list_node_t;

void lip_sexp_print(lip_sexp_t* sexp, int indent);

void lip_sexp_list_init(lip_sexp_list_t* list);
void lip_sexp_list_append(
	lip_sexp_list_t* list,
	lip_sexp_t* element,
	lip_allocator_t* allocator
);
void lip_sexp_list_free(lip_sexp_list_t* list, lip_allocator_t* allocator);
void lip_sexp_free(lip_sexp_t* sexp, lip_allocator_t* allocator);

#endif
