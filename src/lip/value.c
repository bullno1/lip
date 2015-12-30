#include "value.h"
#include <stdio.h>
#include "function.h"

void lip_value_print(lip_value_t* value, int indent, int max_depth)
{
	switch(value->type)
	{
		case LIP_VAL_NIL:
			printf("%*snil", indent * 2, "");
			break;
		case LIP_VAL_NUMBER:
			printf("%*s%f", indent * 2, "", value->data.number);
			break;
		case LIP_VAL_BOOLEAN:
			printf("%*s%s", indent * 2, "", value->data.boolean ? "#t" : "#f");
			break;
		case LIP_VAL_STRING:
			{
				lip_string_t* string = (lip_string_t*)value->data.reference;
				printf("%*s\"%.*s\"", indent * 2, "", string->length, string->ptr);
			}
			break;
		case LIP_VAL_CONS:
			if(max_depth > 1)
			{
				lip_cons_t* cons = (lip_cons_t*)value->data.reference;
				printf("%*s( ", indent * 2, "");
				lip_value_print(&cons->car, 0, max_depth - 1);
				printf("\n%*s. ", indent * 2, "");
				lip_value_print(&cons->cdr, 0, max_depth - 1);
				printf("%*s)", indent * 2, "");
			}
			else
			{
				printf("%*scons: 0x", indent * 2, "");
				unsigned char* p = (unsigned char*)value->data.reference;
				for(int i = 0; i < sizeof(lip_native_function_t); ++i)
				{
					printf("%02x", p[i]);
				}
			}
			break;
		case LIP_VAL_CLOSURE:
			{
				lip_closure_t* closure = (lip_closure_t*)value->data.reference;
				lip_closure_print(closure, indent, max_depth - 1);
			}
			break;
		case LIP_VAL_PLACEHOLDER:
			{
				printf(
					"%*splaceholder: %d",
					indent * 2, "",
					value->data.integer
				);
			}
			break;
	}
}
