#include "function.h"
#include <stdio.h>
#include "asm.h"
#include "value.h"
#include "utils.h"

void lip_closure_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_closure_t* closure, int max_depth, int indent
)
{
	if(closure->info.is_native)
	{
		lip_printf(write_fn, ctx, "%*snative: 0x", indent * 2, "");
		unsigned char* p = (unsigned char*)&closure->function_ptr.native;
		for(int i = 0; i < sizeof(lip_native_function_t); ++i)
		{
			lip_printf(write_fn, ctx, "%02x", p[i]);
		}
	}
	else
	{
		lip_printf(write_fn, ctx, "%*sfunction: 0x", indent * 2, "");
		unsigned char* p = (unsigned char*)&closure->function_ptr.lip;
		for(int i = 0; i < sizeof(lip_function_t*); ++i)
		{
			lip_printf(write_fn, ctx, "%02x", p[i]);
		}

		if(max_depth > 1)
		{
			lip_printf(write_fn, ctx, "\n");
			lip_function_print(
				write_fn, ctx,
				closure->function_ptr.lip, max_depth - 1, indent
			);
			lip_printf(write_fn, ctx, "%*s.ENV:\n", indent * 2, "");
			for(unsigned int i = 0; i < closure->environment_size; ++i)
			{
				lip_printf(write_fn, ctx, "%*s%d:\n", indent * 2 + 1, "", i + 1);
				lip_value_print(
					write_fn, ctx,
					&closure->environment[i], max_depth - 1, indent + 1
				);
				lip_printf(write_fn, ctx, "\n");
			}
		}
	}
}

void lip_function_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_function_t* function, int max_depth, int indent
)
{
	lip_printf(write_fn, ctx, "%*s.TEXT:\n", indent * 2, "");
	for(int i = 0; i < function->num_instructions; ++i)
	{
		lip_printf(write_fn, ctx, "%*s%3d: ", indent * 2 + 1, "", i);
		lip_instruction_t instruction = function->instructions[i];
		lip_asm_print(write_fn, ctx, instruction);
		lip_printf(write_fn, ctx, "\n");
	}

	lip_printf(write_fn, ctx, "%*s.DATA:\n", indent * 2, "");
	for(int i = 0; i < function->num_constants; ++i)
	{
		lip_printf(write_fn, ctx, "%*s%d:\n", indent * 2 + 1, "", i);
		lip_value_print(
			write_fn, ctx,
			&function->constants[i], max_depth - 1, indent + 1
		);
		lip_printf(write_fn, ctx, "\n");
	}

	lip_printf(write_fn, ctx, "%*s.FUNCTIONS:\n", indent * 2, "");
	for(int i = 0; i < function->num_functions; ++i)
	{
		lip_printf(write_fn, ctx, "%*s%d:\n", indent * 2 + 1, "", i);
		lip_function_print(
			write_fn, ctx,
			function->functions[i], max_depth - 1, indent + 1
		);
	}

	lip_printf(write_fn, ctx, "%*s.IMPORTS:\n", indent * 2, "");
	for(int i = 0; i < function->num_imports; ++i)
	{
		lip_string_t* symbol = function->import_symbols[i];
		lip_printf(write_fn, ctx,
			"%*s%d: %.*s\n",
			indent * 2 + 1, "",
			i,
			symbol->length, symbol->ptr
		);
		lip_value_print(
			write_fn, ctx,
			&function->import_values[i], max_depth - 1, indent + 1
		);
		lip_printf(write_fn, ctx, "\n");
	}
}
