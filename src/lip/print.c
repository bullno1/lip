#include "lip_internal.h"
#include <lip/print.h>
#include <inttypes.h>
#include <lip/io.h>
#include <lip/vm.h>
#include <lip/asm.h>
#include <lip/memory.h>
#include <lip/array.h>

void
lip_print_instruction(
	lip_out_t* output,
	lip_instruction_t instr
)
{
	lip_opcode_t opcode;
	lip_operand_t operand;
	lip_disasm(instr, &opcode, &operand);

	switch((uint8_t)opcode)
	{
		case LIP_OP_NOP:
		case LIP_OP_NIL:
		case LIP_OP_RET:
			lip_printf(
				output, "%*s",
				-4, lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1
			);
			break;
		case LIP_OP_CLS:
			{
				int function_index = operand & 0xFFF;
				int num_captures = (operand >> 12) & 0xFFF;

				lip_printf(
					output, "%*s %d, %d",
					-4, lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
					function_index, num_captures
				);
			}
			break;
		case LIP_OP_LABEL:
			lip_printf(
				output, "%*s %d",
				-4,
				"LBL",
				operand
			);
			break;
		default:
			{
				const char* opcode_name = lip_opcode_t_to_str(opcode);
				lip_printf(
					output, "%*s %d",
					-4,
					opcode_name ? opcode_name + sizeof("LIP_OP_") - 1 : "ILL",
					operand
				);
			}
			break;
	}
}

void
lip_print_value(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_value_t value
)
{
	switch(value.type)
	{
		case LIP_VAL_NIL:
			lip_printf(output, "nil\n");
			break;
		case LIP_VAL_NUMBER:
			{
				double dnum = value.data.number;
				int64_t inum = (int64_t)dnum;
				if((double)inum == dnum)
				{
					lip_printf(output, "%zd\n", inum);
				}
				else
				{
					lip_printf(output, "%f\n", dnum);
				}
			}
			break;
		case LIP_VAL_BOOLEAN:
			lip_printf(
				output, "%s\n", value.data.boolean ? "true" : "false"
			);
			break;
		case LIP_VAL_STRING:
			{
				lip_string_t* string = value.data.reference;
				lip_printf(
					output, "\"%.*s\"\n", (int)string->length, string->ptr
				);
			}
			break;
		case LIP_VAL_SYMBOL:
			{
				lip_string_t* string = value.data.reference;
				lip_printf(
					output, "'%.*s\n", (int)string->length, string->ptr
				);
			}
			break;
		case LIP_VAL_LIST:
			lip_print_list(depth, indent, output, value.data.reference);
			break;
		case LIP_VAL_FUNCTION:
			lip_print_closure(depth, indent, output, value.data.reference);
			break;
		case LIP_VAL_PLACEHOLDER:
			lip_printf(output, "<placeholder: #%u>\n", value.data.index);
			break;
		case LIP_VAL_NATIVE:
			lip_printf(
				output, "<native: 0x%" PRIxPTR ">\n",
				(uintptr_t)value.data.reference
			);
			break;
		default:
			lip_printf(output, "<corrupted: #%u>\n", value.type);
			break;
	}
}

void
lip_print_function(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_function_t* function
)
{
	lip_printf(
		output, "<function: 0x%" PRIxPTR ">\n", (uintptr_t)function
	);

	if(depth == 0) { return; }

	lip_function_layout_t layout;
	lip_function_layout(function, &layout);

	lip_printf(
		output, "%*sSource: %.*s\n",
		indent * 2, "", (int)layout.source_name->length, layout.source_name->ptr
	);

	lip_printf(
		output, "%*sArity: %u\n",
		indent * 2, "", function->num_args
	);
	lip_printf(
		output, "%*sNum locals: %u\n",
		indent * 2, "", function->num_locals
	);

	if(function->num_imports > 0)
	{
		lip_printf(output, "%*sImports:\n", indent * 2, "");
	}

	for(uint16_t i = 0; i < function->num_imports; ++i)
	{
		lip_import_t import = layout.imports[i];
		lip_string_t* import_name =
			lip_function_resource(function, import.name);
		lip_printf(
			output, "%*s%u: %.*s\n",
			indent * 2 + 2, "", i, (int)import_name->length, import_name->ptr
		);
	}

	if(function->num_constants > 0)
	{
		lip_printf(output, "%*sConstants:\n", indent * 2, "");
	}

	for(uint16_t i = 0; i < function->num_constants; ++i)
	{
		lip_printf(output, "%*s%u: ", indent * 2 + 2, "", i);
		switch(layout.constants[i].type)
		{
			case LIP_VAL_NUMBER:
				lip_print_value(
					depth - 1, indent + 1, output, layout.constants[i]
				);
				break;
			case LIP_VAL_STRING:
			case LIP_VAL_SYMBOL:
				lip_print_value(
					depth - 1, indent + 1, output,
					(lip_value_t) {
						.type = layout.constants[i].type,
						.data = {
							.reference = lip_function_resource(
								function, layout.constants[i].data.index
							)
						}
					}
				);
				break;
			default:
				lip_printf(output, "<corrupted>\n");
				break;
		}
	}

	lip_printf(output, "%*sCode:\n", indent * 2, "");
	for(uint16_t i = 0; i < function->num_instructions; ++i)
	{
		lip_printf(output, "%*s%*u: ", indent * 2 + 1, "", 3, i);
		lip_print_instruction(output, layout.instructions[i]);
		lip_loc_range_t loc = layout.locations[i + 1];
		lip_printf(output, "%*s; %u:%u - %u:%u\n",
			3, "",
			loc.start.line, loc.start.column, loc.end.line, loc.end.column
		);
	}

	if(function->num_functions > 0)
	{
		lip_printf(output, "%*sFunctions:\n", indent * 2, "");
	}

	for(uint16_t i = 0; i < function->num_functions; ++i)
	{
		lip_printf(output, "%*s%u: ", indent * 2 + 1, "", i);
		lip_print_function(
			depth - 1, indent + 1, output,
			lip_function_resource(function, layout.function_offsets[i])
		);
	}
}

void
lip_print_list(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_list_t* list
)
{
	lip_printf(
		output, "<list: 0x%" PRIxPTR ">\n", (uintptr_t)list
	);

	if(depth == 0) { return; }

	for(size_t i = 0; i < list->length; ++i)
	{
		lip_printf(output, "%*s- ", indent * 2 + 2, "");
		lip_print_value(depth - 1, indent + 1, output, list->elements[i]);
	}
}

void
lip_print_script(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_script_t* script
)
{
	lip_print_closure(depth, indent, output, script->closure);
}

void
lip_print_closure(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_closure_t* closure
)
{
	lip_printf(
		output, "<closure: 0x%" PRIxPTR ">\n", (uintptr_t)closure
	);

	if(depth == 0) { return; }

	lip_printf(
		output, "%*sNative: %s\n",
		indent * 2 + 1, "", closure->is_native ? "true" : "false");

	if(closure->env_len > 0)
	{
		lip_printf(output, "%*sEnvironment:\n", indent * 2 + 1, "");
	}

	for(uint16_t i = 0; i < closure->env_len; ++i)
	{
		lip_printf(output, "%*s%u: ", indent * 2 + 2, "", i);
		lip_print_value(depth - 1, indent + 1, output, closure->environment[i]);
	}

	lip_printf(output, "%*sFunction: ", indent * 2 + 1, "");
	if(closure->is_native)
	{
		lip_printf(
			output, "0x%" PRIxPTR "\n",
			(uintptr_t)closure->function.native
		);
	}
	else
	{
		lip_print_function(
			depth - 1, indent + 1, output, closure->function.lip
		);
	}
}

static void
lip_print_ast_block(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_array(lip_ast_t*) ast_block
)
{
	lip_array_foreach(lip_ast_t*, ast, ast_block)
	{
		lip_printf(output, "%*s", indent * 2, "");
		lip_print_ast(depth, indent, output, *ast);
	}
}

void
lip_print_ast(
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_ast_t* ast
)
{
	lip_printf(
		output, "%s (%u:%u - %u:%u)",
		lip_ast_type_t_to_str(ast->type) + sizeof("LIP_AST_") - 1,
		ast->location.start.line, ast->location.start.column,
		ast->location.end.line, ast->location.end.column
	);

	if(depth == 0)
	{
		lip_printf(output, "\n");
		return;
	}
	else
	{
		lip_printf(output, ": ");
	}

	switch(ast->type)
	{
		case LIP_AST_IDENTIFIER:
			lip_printf(
				output, "%.*s\n",
				(int)ast->data.string.length, ast->data.string.ptr
			);
			break;
		case LIP_AST_IF:
			lip_printf(
				output, "\n%*sCondition: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				depth - 1, indent + 1, output,
				ast->data.if_.condition
			);
			lip_printf(
				output, "%*sThen: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				depth - 1, indent + 1, output,
				ast->data.if_.then
			);
			if(ast->data.if_.else_)
			{
				lip_printf(
					output, "%*sElse: ", indent * 2 + 1, ""
				);
				lip_print_ast(
					depth - 1, indent + 1, output,
					ast->data.if_.else_
				);
			}
			break;
		case LIP_AST_APPLICATION:
			lip_printf(
				output, "\n%*sFunction: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				depth - 1, indent + 1, output,
				ast->data.application.function
			);
			lip_printf(
				output, "%*sArguments:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				depth - 1, indent + 1, output,
				ast->data.application.arguments
			);
			break;
		case LIP_AST_LAMBDA:
			lip_printf(
				output, "\n%*sArguments:", indent * 2 + 1, ""
			);
			lip_array_foreach(lip_string_ref_t, arg, ast->data.lambda.arguments)
			{
				lip_printf(
					output, " %.*s", (int)arg->length, arg->ptr
				);
			}
			lip_printf(
				output, "\n%*sBody:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				depth - 1, indent + 1, output,
				ast->data.lambda.body
			);
			break;
		case LIP_AST_DO:
			lip_printf(output, "\n");
			lip_print_ast_block(depth - 1, indent + 1, output, ast->data.do_);
			break;
		case LIP_AST_LET:
		case LIP_AST_LETREC:
			lip_printf(
				output, "\n%*sBindings:\n", indent * 2 + 1, ""
			);
			lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
			{
				lip_printf(
					output, "%*s%.*s: ",
					indent * 2 + 2, "",
					(int)binding->name.length, binding->name.ptr
				);
				lip_print_ast(
					depth - 1, indent + 2, output, binding->value
				);
			}
			lip_printf(
				output, "%*sBody:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				depth - 1, indent + 1, output, ast->data.let.body
			);
			break;
		case LIP_AST_STRING:
			lip_printf(
				output, "\"%.*s\"\n",
				(int)ast->data.string.length, ast->data.string.ptr
			);
			break;
		case LIP_AST_SYMBOL:
			lip_printf(
				output, "'%.*s\n",
				(int)ast->data.string.length, ast->data.string.ptr
			);
			break;
		case LIP_AST_NUMBER:
			lip_printf(
				output, "%f\n",
				ast->data.number
			);
			break;
	}
}
