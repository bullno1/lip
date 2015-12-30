#include "function.h"
#include <stdio.h>
#include "asm.h"
#include "value.h"

void lip_closure_print(lip_closure_t* closure, int indent, int max_depth)
{
	if(closure->info.is_native)
	{
		printf("%*snative: 0x", indent * 2, "");
		unsigned char* p = (unsigned char*)&closure->function_ptr.native;
		for(int i = 0; i < sizeof(lip_native_function_t); ++i)
		{
			printf("%02x", p[i]);
		}
	}
	else
	{
		if(max_depth > 1)
		{
			lip_function_print(closure->function_ptr.lip, indent, max_depth - 1);
			printf("%*s.ENV:\n", indent * 2, "");
			for(unsigned int i = 0; i < closure->environment_size; ++i)
			{
				printf("%*s%d:\n", indent * 2 + 1, "", i + 1);
				lip_value_print(
					&closure->environment[i], indent + 1, max_depth - 1
				);
				printf("\n");
			}
		}
		else
		{
			printf("%*sfunction: 0x", indent * 2, "");
			unsigned char* p = (unsigned char*)&closure->function_ptr.lip;
			for(int i = 0; i < sizeof(lip_function_t*); ++i)
			{
				printf("%02x", p[i]);
			}
		}
	}
}

void lip_function_print(lip_function_t* function, int indent, int max_depth)
{
	printf("%*s.TEXT:\n", indent * 2, "");
	for(int i = 0; i < function->num_instructions; ++i)
	{
		printf("%*s%3d: ", indent * 2 + 1, "", i);
		lip_instruction_t instruction = function->instructions[i];
		lip_asm_print(instruction);
		printf("\n");
	}

	printf("%*s.DATA:\n", indent * 2, "");
	for(int i = 0; i < function->num_constants; ++i)
	{
		printf("%*s%d:\n", indent * 2 + 1, "", i);
		lip_value_print(&function->constants[i], indent + 1, max_depth - 1);
		printf("\n");
	}

	printf("%*s.FUNCTIONS:\n", indent * 2, "");
	for(int i = 0; i < function->num_functions; ++i)
	{
		printf("%*s%d:\n", indent * 2 + 1, "", i);
		lip_function_print(function->functions[i], indent + 1, max_depth - 1);
	}

	printf("%*s.IMPORTS:\n", indent * 2, "");
	for(int i = 0; i < function->num_imports; ++i)
	{
		lip_string_t* symbol = function->import_symbols[i];
		printf(
			"%*s%d: %.*s\n",
			indent * 2 + 1, "",
			i,
			symbol->length, symbol->ptr
		);
		lip_value_print(&function->import_values[i], indent + 1, max_depth - 1);
		printf("\n");
	}
}
