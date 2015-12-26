#include "sexp.h"
#include <stdio.h>
#include "allocator.h"

void lip_sexp_print(lip_sexp_t* sexp, int indent)
{
	switch(sexp->type)
	{
	case LIP_SEXP_LIST:
		printf(
			"%*s%s(%d) %u:%u - %u:%u",
			indent * 2, "",
			lip_sexp_type_t_to_str(sexp->type),
			(int)sexp->data.list.length,
			sexp->start.line, sexp->start.column,
			sexp->end.line, sexp->end.column
		);
		for(
			lip_sexp_list_node_t* itr = sexp->data.list.first;
			itr != NULL;
			itr = itr->next
		)
		{
			printf("\n");
			lip_sexp_print(&itr->data, indent + 1);
		}
		break;
	case LIP_SEXP_SYMBOL:
	case LIP_SEXP_STRING:
	case LIP_SEXP_NUMBER:
		printf(
			"%*s%s '%.*s' %u:%u - %u:%u",
			indent * 2, "",
			lip_sexp_type_t_to_str(sexp->type),
			(int)sexp->data.string.length, sexp->data.string.ptr,
			sexp->start.line, sexp->start.column,
			sexp->end.line, sexp->end.column
		);
		break;
	}
}

void lip_sexp_list_init(lip_sexp_list_t* list)
{
	list->length = 0;
	list->first = NULL;
	list->last = NULL;
}

void lip_sexp_list_append(
	lip_sexp_list_t* list,
	lip_sexp_t* element,
	lip_allocator_t* allocator
)
{
	lip_sexp_list_node_t* node = LIP_NEW(allocator, lip_sexp_list_node_t);
	node->data = *element;
	node->next = NULL;

	if(list->first == NULL)
	{
		list->first = node;
		list->last = node;
	}
	else
	{
		list->last->next = node;
		list->last = node;
	}
	++list->length;
}

void lip_sexp_list_free(lip_sexp_list_t* list, lip_allocator_t* allocator)
{
	for(
		lip_sexp_list_node_t* itr = list->first;
		itr != NULL;
	)
	{
		lip_sexp_list_node_t* next = itr->next;
		lip_sexp_free(&itr->data, allocator);
		LIP_DELETE(allocator, itr);
		itr = next;
	}
}

void lip_sexp_free(lip_sexp_t* sexp, lip_allocator_t* allocator)
{
	if(sexp->type == LIP_SEXP_LIST)
	{
		lip_sexp_list_free(&sexp->data.list, allocator);
	}
}
