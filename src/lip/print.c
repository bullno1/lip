#include "print.h"
#include <inttypes.h>
#include "io.h"
#include "ex/vm.h"
#include "asm.h"
#include "memory.h"
#include "array.h"

void
lip_print_instruction(
	lip_allocator_t* allocator,
	lip_out_t* output,
	lip_instruction_t instr
)
{
	lip_opcode_t opcode;
	lip_operand_t operand;
	lip_disasm(instr, &opcode, &operand);

	switch(opcode)
	{
		case LIP_OP_NOP:
		case LIP_OP_NIL:
		case LIP_OP_RET:
			lip_printf(
				allocator, output, "%*s\n",
				-4, lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1
			);
			break;
		case LIP_OP_CLS:
			{
				int function_index = operand & 0xFFF;
				int num_captures = (operand >> 12) & 0xFFF;

				lip_printf(
					allocator, output, "%*s %d, %d\n",
					-4, lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
					function_index, num_captures
				);
			}
			break;
		default:
			lip_printf(
				allocator, output, "%*s %d\n",
				-4, lip_opcode_t_to_str(opcode) + sizeof("LIP_OP_") - 1,
				operand
			);
			break;
	}
}

void
lip_print_value(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_value_t value
)
{
	switch(value.type)
	{
		case LIP_VAL_NIL:
			lip_printf(allocator, output, "nil\n");
			break;
		case LIP_VAL_NUMBER:
			lip_printf(allocator, output, "%f\n", value.data.number);
			break;
		case LIP_VAL_BOOLEAN:
			lip_printf(
				allocator, output, "%s\n", value.data.boolean ? "#t" : "#f"
			);
			break;
		case LIP_VAL_STRING:
			{
				lip_string_ref_t* string = value.data.reference;
				lip_printf(
					allocator, output, "\"%.*s\"\n", (int)string->length, string->ptr
				);
			}
			break;
		case LIP_VAL_FUNCTION:
			lip_print_closure(
				allocator, depth, indent, output, value.data.reference
			);
			break;
		case LIP_VAL_PLACEHOLDER:
			lip_printf(allocator, output, "<placeholder: #%u>\n", value.data.index);
			break;
	}
}

void
lip_print_function(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_function_t* function
)
{
	lip_printf(
		allocator, output, "<function: 0x%" PRIxPTR ">\n", (uintptr_t)function
	);

	if(depth == 0) { return; }

	lip_function_layout_t layout;
	lip_function_layout(function, &layout);

	lip_printf(
		allocator, output, "%*sSource: %.*s\n",
		indent * 2 + 1, "", (int)layout.source_name->length, layout.source_name->ptr
	);

	lip_printf(
		allocator, output, "%*sArity: %u\n",
		indent * 2 + 1, "", function->num_args
	);

	lip_printf(allocator, output, "%*sImports:\n", indent * 2 + 1, "");
	for(uint16_t i = 0; i < function->num_imports; ++i)
	{
		lip_import_t import = layout.imports[i];
		lip_string_ref_t* import_name =
			lip_function_resource(function, import.name);
		lip_printf(
			allocator, output, "%*s%.*s: ",
			indent * 2 + 1, "", (int)import_name->length, import_name->ptr
		);
		lip_print_value(allocator, depth - 1, indent + 1, output, import.value);
	}

	lip_printf(allocator, output, "%*sConstants:\n", indent * 2 + 1, "");
	for(uint16_t i = 0; i < function->num_constants; ++i)
	{
		lip_printf(allocator, output, "%*s%u: ", indent * 2 + 2, "", i);
		lip_print_value(
			allocator, depth - 1, indent + 1, output, layout.constants[i]
		);
	}

	lip_printf(allocator, output, "%*sFunctions:\n", indent * 2 + 1, "");
	for(uint16_t i = 0; i < function->num_functions; ++i)
	{
		lip_printf(allocator, output, "%*s%u: ", indent * 2 + 2, "", i);
		lip_print_function(
			allocator, depth - 1, indent + 1, output,
			lip_function_resource(function, layout.function_offsets[i])
		);
	}

	lip_printf(allocator, output, "%*sCode:\n", indent * 2 + 1, "");
	for(uint16_t i = 0; i < function->num_instructions; ++i)
	{
		lip_printf(allocator, output, "%*s%*u: ", indent * 2 + 2, "", 3, i);
		lip_print_instruction(allocator, output, layout.instructions[i]);
	}
}

void
lip_print_closure(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_closure_t* closure
)
{
	lip_printf(
		allocator, output, "<closure: 0x%" PRIxPTR ">\n", (uintptr_t)closure
	);

	if(depth == 0) { return; }

	lip_printf(
		allocator, output, "%*sNative: %s\n",
		indent * 2, "", closure->is_native ? "true" : "false");
	lip_printf(allocator, output, "%*sEnvironment:\n", indent * 2, "");
	for(uint16_t i = 0; i < closure->env_len; ++i)
	{
		lip_printf(allocator, output, "%*s%*u: ", indent * 2 + 1, "", 3, i);
		lip_print_value(allocator, depth - 1, indent + 1, output, closure->environment[i]);
	}

	lip_printf(allocator, output, "%*sFunction: ", indent * 2, "");
	if(closure->is_native)
	{
		lip_printf(
			allocator, output, "0x%" PRIxPTR "\n",
			(uintptr_t)closure->function.native
		);
	}
	else
	{
		lip_print_function(
			allocator, depth - 1, indent + 1, output, closure->function.lip
		);
	}
}

static void
lip_print_ast_block(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	lip_array(lip_ast_t*) ast_block
)
{
	lip_array_foreach(lip_ast_t*, ast, ast_block)
	{
		lip_printf(allocator, output, "%*s", indent * 2, "");
		lip_print_ast(allocator, depth, indent, output, *ast);
	}
}

void
lip_print_ast(
	lip_allocator_t* allocator,
	unsigned int depth,
	unsigned int indent,
	lip_out_t* output,
	const lip_ast_t* ast
)
{
	lip_printf(
		allocator, output, "%s",
		lip_ast_type_t_to_str(ast->type) + sizeof("LIP_AST_") - 1
	);

	if(depth == 0)
	{
		lip_printf(allocator, output, "\n");
		return;
	}
	else
	{
		lip_printf(allocator, output, ": ");
	}

	switch(ast->type)
	{
		case LIP_AST_IDENTIFIER:
			lip_printf(
				allocator, output, "%.*s\n",
				(int)ast->data.string.length, ast->data.string.ptr
			);
			break;
		case LIP_AST_IF:
			lip_printf(
				allocator, output, "\n%*sCondition: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				allocator, depth - 1, indent + 1, output,
				ast->data.if_.condition
			);
			lip_printf(
				allocator, output, "%*sThen: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				allocator, depth - 1, indent + 1, output,
				ast->data.if_.then
			);
			if(ast->data.if_.else_)
			{
				lip_printf(
					allocator, output, "%*sElse: ", indent * 2 + 1, ""
				);
				lip_print_ast(
					allocator, depth - 1, indent + 1, output,
					ast->data.if_.else_
				);
			}
			break;
		case LIP_AST_APPLICATION:
			lip_printf(
				allocator, output, "\n%*sFunction: ", indent * 2 + 1, ""
			);
			lip_print_ast(
				allocator, depth - 1, indent + 1, output,
				ast->data.application.function
			);
			lip_printf(
				allocator, output, "%*sArguments:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				allocator, depth - 1, indent + 1, output,
				ast->data.application.arguments
			);
			break;
		case LIP_AST_LAMBDA:
			lip_printf(
				allocator, output, "\n%*sArguments:", indent * 2 + 1, ""
			);
			lip_array_foreach(lip_string_ref_t, arg, ast->data.lambda.arguments)
			{
				lip_printf(
					allocator, output, " %.*s", (int)arg->length, arg->ptr
				);
			}
			lip_printf(
				allocator, output, "\n%*sBody:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				allocator, depth - 1, indent + 1, output,
				ast->data.lambda.body
			);
			break;
		case LIP_AST_DO:
			lip_printf(allocator, output, "\n");
			lip_print_ast_block(
				allocator, depth - 1, indent + 1, output,
				ast->data.lambda.body
			);
			break;
		case LIP_AST_LET:
		case LIP_AST_LETREC:
			lip_printf(
				allocator, output, "\n%*sBindings:\n", indent * 2 + 1, ""
			);
			lip_array_foreach(lip_let_binding_t, binding, ast->data.let.bindings)
			{
				lip_printf(
					allocator, output, "%*s%.*s: ",
					indent * 2 + 2, "",
					(int)binding->name.length, binding->name.ptr
				);
				lip_print_ast(
					allocator, depth - 1, indent + 2, output, binding->value
				);
			}
			lip_printf(
				allocator, output, "%*sBody:\n", indent * 2 + 1, ""
			);
			lip_print_ast_block(
				allocator, depth - 1, indent + 1, output, ast->data.let.body
			);
			break;
		case LIP_AST_STRING:
			lip_printf(
				allocator, output, "\"%.*s\"\n",
				(int)ast->data.string.length, ast->data.string.ptr
			);
			break;
		case LIP_AST_NUMBER:
			lip_printf(
				allocator, output, "%f\n",
				ast->data.number
			);
			break;
	}
}
