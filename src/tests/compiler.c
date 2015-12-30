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

typedef struct stack_allocator_t
{
	lip_allocator_t vtable;
	lip_allocator_t* backing_allocator;
	size_t index;
	size_t max;
	char stack[];
} stack_allocator_t;

typedef struct mem_block
{
	size_t size;
	char mem[];
} mem_block;

void* stack_alloc(void* self, size_t size)
{
	stack_allocator_t* allocator = self;
	size_t bump = (size + (sizeof(void*) - 1)) / sizeof(void*) * sizeof(void*);
	if(allocator->max - allocator->index >= bump)
	{
		void* mem = allocator->stack + allocator->index;
		allocator->index += bump;
		return mem;
	}
	else
	{
		return NULL;
	}
}

void* stack_realloc(void* self, void* old, size_t size)
{
	mem_block* block = stack_alloc(self, sizeof(mem_block) + size);
	if(block == NULL) { return NULL; }
	block->size = size;
	if(old != NULL)
	{
		mem_block* old_block = (mem_block*)((char*)old - offsetof(mem_block, mem));
		memcpy(block->mem, old_block->mem, old_block->size);
	}
	return block->mem;
}

void stack_free(void* self, void* ptr)
{
}

lip_allocator_t* stack_allocator_new(
	lip_allocator_t* allocator, size_t heap_size
)
{
	size_t allocator_size = sizeof(stack_allocator_t) + heap_size;
	stack_allocator_t* stack_allocator = lip_malloc(allocator, allocator_size);
	stack_allocator->backing_allocator = allocator;
	stack_allocator->index = 0;
	stack_allocator->max = heap_size;
	stack_allocator->vtable.realloc = stack_realloc;
	stack_allocator->vtable.free = stack_free;
	return &stack_allocator->vtable;
}

void stack_allocator_delete(lip_allocator_t* allocator)
{
	stack_allocator_t* stack_allocator = (stack_allocator_t*)allocator;
	lip_free(stack_allocator->backing_allocator, stack_allocator);
}

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
	lip_allocator_t* vm_allocator =
		stack_allocator_new(lip_default_allocator, 1024*1024);
	size_t mem_required = lip_vm_config(&vm, vm_allocator, 256, 256, 256);
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
			if(lip_compiler_add_sexp(&compiler, &parse_result.sexp))
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

	stack_allocator_delete(vm_allocator);
	lip_module_free(lip_default_allocator, builtins);
	lip_free(lip_default_allocator, vm_mem);
	lip_linker_cleanup(&linker);
	lip_compiler_cleanup(&compiler);
	lip_bundler_cleanup(&bundler);
	lip_lexer_cleanup(&lexer);

	return EXIT_SUCCESS;
}
