#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include <lip/allocator.h>
#include <lip/parser.h>
#include <lip/lexer.h>
#include <lip/compiler.h>
#include <lip/module.h>
#include <lip/vm.h>
#include <lip/linker.h>
#include <lip/bundler.h>
#include <lip/utils.h>

lip_exec_status_t print(lip_vm_t* vm)
{
	lip_value_print(lip_vm_get_arg(vm, 0), 0);
	printf("\n");
	lip_vm_push_nil(vm);
	return LIP_EXEC_OK;
}

int main(int argc, char* argv[])
{
	lip_lexer_t lexer;
	lip_lexer_init(&lexer, lip_default_allocator, read_file, stdin);

	lip_parser_t parser;
	lip_parser_init(&parser, &lexer, lip_default_allocator);

	lip_parse_status_t parse_status;
	lip_parse_result_t parse_result;

	lip_compiler_t compiler;
	lip_compiler_init(&compiler, lip_default_allocator, write_file, stderr);

	lip_linker_t linker;
	lip_linker_init(&linker, lip_default_allocator);

	lip_vm_t vm;
	size_t mem_required = lip_vm_config(&vm, 256, 256, 256);
	void* vm_mem = lip_malloc(lip_default_allocator, mem_required);
	lip_vm_init(&vm, vm_mem);

	lip_bundler_t bundler;
	lip_bundler_init(&bundler, lip_default_allocator);
	lip_bundler_begin(&bundler);
	lip_bundler_add_native_function(&bundler, lip_string_ref("print"), print, 1);
	lip_module_t* builtins = lip_bundler_end(&bundler);

	while(true)
	{
		parse_status = lip_parser_next_sexp(&parser, &parse_result);
		if(parse_status == LIP_PARSE_OK)
		{
			lip_sexp_print(&parse_result.sexp, 0);
			printf("\n");

			lip_compiler_begin(&compiler, LIP_COMPILE_MODE_REPL);
			lip_compile_status_t compile_status =
				lip_compiler_add_sexp(&compiler, &parse_result.sexp);
			if(compile_status == LIP_COMPILE_OK)
			{
				lip_module_t* module = lip_compiler_end(&compiler);
				lip_module_print(module);

				lip_linker_reset(&linker);
				lip_linker_add_module(&linker, builtins);
				lip_linker_add_module(&linker, module);
				lip_linker_link_modules(&linker);

				lip_value_t main_symbol;
				lip_linker_find_symbol(
					&linker, lip_string_ref("main"), &main_symbol
				);
				lip_vm_push_value(&vm, &main_symbol);
				lip_vm_call(&vm, 0);
				lip_value_print(lip_vm_pop(&vm), 0);
				printf("\n");

				lip_module_free(lip_default_allocator, module);
				printf("\n");
			}

			lip_sexp_cleanup(&parse_result.sexp);
		}
		else
		{
			lip_parser_print_status(parse_status, &parse_result);
			printf("\n");
		}

		if(parse_status == LIP_PARSE_EOS)
		{
			break;
		}

		lip_lexer_reset(&lexer);
	}

	lip_module_free(lip_default_allocator, builtins);
	lip_free(lip_default_allocator, vm_mem);
	lip_linker_cleanup(&linker);
	lip_compiler_cleanup(&compiler);
	lip_bundler_cleanup(&bundler);
	lip_lexer_cleanup(&lexer);

	return EXIT_SUCCESS;
}
