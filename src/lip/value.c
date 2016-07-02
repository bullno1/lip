#include "value.h"
#include <stdio.h>
#include "function.h"
#include "utils.h"

void lip_value_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_value_t* value, int max_depth, int indent
)
{
	switch(value->type)
	{
		case LIP_VAL_NIL:
			lip_printf(write_fn, ctx, "%*snil", indent * 2, "");
			break;
		case LIP_VAL_NUMBER:
			lip_printf(write_fn, ctx,
				"%*s%f", indent * 2, "", value->data.number);
			break;
		case LIP_VAL_BOOLEAN:
			lip_printf(write_fn, ctx,
				"%*s%s", indent * 2, "", value->data.boolean ? "#t" : "#f");
			break;
		case LIP_VAL_STRING:
			{
				lip_string_t* string = (lip_string_t*)value->data.reference;
				lip_printf(write_fn, ctx,
					"%*s\"%.*s\"", indent * 2, "", string->length, string->ptr);
			}
			break;
		case LIP_VAL_CONS:
			if(max_depth > 1)
			{
				lip_cons_t* cons = (lip_cons_t*)value->data.reference;
				lip_printf(write_fn, ctx, "%*s( ", indent * 2, "");
				lip_value_print(write_fn, ctx, &cons->car, max_depth - 1, 0);
				lip_printf(write_fn, ctx, "\n%*s. ", indent * 2, "");
				lip_value_print(write_fn, ctx, &cons->cdr, max_depth - 1, 0);
				lip_printf(write_fn, ctx, "%*s)", indent * 2, "");
			}
			else
			{
				lip_printf(write_fn, ctx, "%*scons: 0x", indent * 2, "");
				unsigned char* p = (unsigned char*)value->data.reference;
				for(size_t i = 0; i < sizeof(lip_native_function_t); ++i)
				{
					lip_printf(write_fn, ctx, "%02x", p[i]);
				}
			}
			break;
		case LIP_VAL_CLOSURE:
			{
				lip_closure_t* closure = (lip_closure_t*)value->data.reference;
				lip_closure_print(write_fn, ctx, closure, max_depth - 1, indent);
			}
			break;
		case LIP_VAL_PLACEHOLDER:
			{
				lip_printf(write_fn, ctx,
					"%*splaceholder: %d",
					indent * 2, "",
					value->data.integer
				);
			}
			break;
	}
}
