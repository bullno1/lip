#include "sexp.h"
#include <stdio.h>
#include "array.h"

void lip_sexp_print(lip_sexp_t* sexp, int indent)
{
	switch(sexp->type)
	{
	case LIP_SEXP_LIST:
		printf(
			"%*s%s(%d) %u:%u - %u:%u",
			indent * 2, "",
			lip_sexp_type_t_to_str(sexp->type) + sizeof("LIP_SEXP_") - 1,
			(int)lip_array_len(sexp->data.list),
			sexp->start.line, sexp->start.column,
			sexp->end.line, sexp->end.column
		);
		lip_array_foreach(lip_sexp_t, itr, sexp->data.list)
		{
			printf("\n");
			lip_sexp_print(itr, indent + 1);
		}
		break;
	case LIP_SEXP_SYMBOL:
	case LIP_SEXP_STRING:
	case LIP_SEXP_NUMBER:
		printf(
			"%*s%s '%.*s' %u:%u - %u:%u",
			indent * 2, "",
			lip_sexp_type_t_to_str(sexp->type) + sizeof("LIP_SEXP_") - 1,
			(int)sexp->data.string.length, sexp->data.string.ptr,
			sexp->start.line, sexp->start.column,
			sexp->end.line, sexp->end.column
		);
		break;
	}
}

static void lip_sexp_cleanup_list(lip_sexp_t* list)
{
	lip_array_foreach(lip_sexp_t, itr, list)
	{
		lip_sexp_cleanup(itr);
	}
}

void lip_sexp_cleanup(lip_sexp_t* sexp)
{
	if(sexp->type == LIP_SEXP_LIST)
	{
		lip_sexp_cleanup_list(sexp->data.list);
		lip_array_delete(sexp->data.list);
	}
}
